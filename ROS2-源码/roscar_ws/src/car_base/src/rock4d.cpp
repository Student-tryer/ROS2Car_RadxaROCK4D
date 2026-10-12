#include "car_base/rock4d.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <tf2/LinearMath/Quaternion.h>

namespace
{
    constexpr uint8_t ODOM_HEAD = 0xA1; //constexpr为编译期常量，即该值在编译是已经确定，写死在程序里，运行时无法改动
    constexpr uint8_t ODOM_TILE = 0xB1;
    constexpr size_t  ODOM_LEN  = 14;
    constexpr uint8_t VOLT_HEAD = 0xA2;
    constexpr uint8_t VOLT_TILE = 0xB2;
    constexpr size_t  VOLT_LEN  = 6;
}

/* ==================== 生命周期 ==================== */

Rock4DNode::Rock4DNode() : Node("rock4d_driver")
{
  port_ = declare_parameter<std::string>("port", "/dev/ttyUSB0");
  baud_ = declare_parameter<int>("baud", 115200);
  wheel_base_m_ = declare_parameter<double>("wheel_base", 0.16872);
  wheel_circ_m_ = declare_parameter<double>("wheel_circumference", 0.21834);
  encoder_ppr_ = declare_parameter<int>("encoder_ppr", 336);
  loop_hz_ = declare_parameter<int>("loop_hz", 200);
  request_hz_ = declare_parameter<int>("request_hz", 50);

  /* 每脉冲对应位移 (m) = 轮周长(m) / PPR */
  tick_to_m_ = wheel_circ_m_ / (encoder_ppr_);

  if (!open_serial()) {
    RCLCPP_ERROR(get_logger(), "open serial %s failed", port_.c_str());
    throw std::runtime_error("serial open failed");
  }

  odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom", 10);
  batt_pub_ = create_publisher<std_msgs::msg::Float32>("battery_voltage", 10);
  cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
    "cmd_vel", 10,
    std::bind(&Rock4DNode::cmd_vel_cb, this, std::placeholders::_1));
  tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);

  poll_timer_ = create_wall_timer(
    std::chrono::milliseconds(1000 / request_hz_),
    std::bind(&Rock4DNode::poll_cb, this));

  //send_control(2, 2);   /* 请求电压一次 */
  //usleep(10);
  send_control(1, 1);   /* 解除急停 */
  usleep(1000);
  /* 测试模式: 启动时自动下发指定线速度 (m/s), 0=不动作 */
  double test_speed = declare_parameter<double>("test_speed", 0.0);
  if (test_speed > 0.0) {
    send_cmd_vel(test_speed, 0.0);
    usleep(1000);
    RCLCPP_INFO(get_logger(), "TEST: auto send v=%.2f m/s", test_speed);
  }

  RCLCPP_INFO(get_logger(), "driver on %s @ %d, poll=%dHz, tick_to_m=%.4f",
              port_.c_str(), baud_, request_hz_, tick_to_m_);
}

/* ==================== 串口底层 (POSIX) ==================== */

bool Rock4DNode::open_serial()
{
  serial_fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (serial_fd_ < 0) return false;

  termios tio{};
  tcgetattr(serial_fd_, &tio);
  cfmakeraw(&tio);
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 0;
  speed_t speed = B115200;
  if (baud_ == 9600)  speed = B9600;
  else if (baud_ == 57600)  speed = B57600;
  else if (baud_ == 230400) speed = B230400;
  else if (baud_ != 115200) {
    RCLCPP_ERROR(get_logger(), "unsupported baud: %d", baud_);
    ::close(serial_fd_);
    return false;
  }
  cfsetispeed(&tio, speed);
  cfsetospeed(&tio, speed);
  tcsetattr(serial_fd_, TCSANOW, &tio);
  tcflush(serial_fd_, TCIOFLUSH);
  return true;
}

ssize_t Rock4DNode::write_bytes(const uint8_t * buf, size_t len)
{
  std::lock_guard<std::mutex> lock(serial_mutex_);
  return ::write(serial_fd_, buf, len);
}

void Rock4DNode::drain_serial()
{
  uint8_t buf[128];
  ssize_t n;
  while ((n = ::read(serial_fd_, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; ++i) on_rx_byte(buf[i]);
  }
}

/* ==================== 协议: 下行 ==================== */

void Rock4DNode::send_cmd_vel(double v, double w)
{
  uint8_t frame[12];
  float fv = static_cast<float>(v);
  float fw = static_cast<float>(w);
  uint32_t uv, uw;
  std::memcpy(&uv, &fv, 4);
  std::memcpy(&uw, &fw, 4);

  frame[0] = 0xAA; frame[1] = 0x55;
  /* float → 大端 */
  frame[2] = (uv >> 24) & 0xFF; frame[3] = (uv >> 16) & 0xFF;
  frame[4] = (uv >> 8) & 0xFF;  frame[5] = uv & 0xFF;
  frame[6] = (uw >> 24) & 0xFF; frame[7] = (uw >> 16) & 0xFF;
  frame[8] = (uw >> 8) & 0xFF;  frame[9] = uw & 0xFF;
  frame[10] = 0x0D; frame[11] = 0x0A;
  ssize_t n = write_bytes(frame, sizeof(frame));
  char hex[40] = {0};
  for (size_t i = 0; i < sizeof(frame); ++i) {
    std::snprintf(hex + i * 3, 4, "%02X ", frame[i]);
  }
  RCLCPP_INFO(get_logger(), "TX %zd B: %s", n, hex);
}

void Rock4DNode::send_control(uint8_t id, uint8_t data)
{
  uint8_t frame[6] = {0xAA, 0x55, id, data, 0x0D, 0x0A};
  write_bytes(frame, sizeof(frame));
}

/* ==================== 协议: 上行解析 ==================== */

void Rock4DNode::on_rx_byte(uint8_t b)
{
  /* 不在收帧中: 根据帧头判断收哪种帧 */
  if (rx_frame_expected_ == 0) {
    if (b == ODOM_HEAD) {
      rx_frame_[0] = b;
      rx_frame_len_ = 1;
      rx_frame_expected_ = ODOM_LEN;
    } else if (b == VOLT_HEAD) {
      rx_frame_[0] = b;
      rx_frame_len_ = 1;
      rx_frame_expected_ = VOLT_LEN;
    }
    return;
  }

  rx_frame_[rx_frame_len_++] = b;
  if (rx_frame_len_ == rx_frame_expected_) {
    parse_frame(rx_frame_, rx_frame_expected_);
    rx_frame_len_ = 0;
    rx_frame_expected_ = 0;
  }
}

void Rock4DNode::parse_frame(const uint8_t * frame, size_t len)
{
  if (len == ODOM_LEN &&
      frame[0] == ODOM_HEAD && frame[ODOM_LEN - 1] == ODOM_TILE) {
    float yaw = be_bytes_to_float(&frame[1]);
    int32_t pos_l = be_bytes_to_int32(&frame[5]);
    int32_t pos_r = be_bytes_to_int32(&frame[9]);
    publish_odom(yaw, pos_l, pos_r);
  } else if (len == VOLT_LEN &&
             frame[0] == VOLT_HEAD && frame[VOLT_LEN - 1] == VOLT_TILE) {
    float v = be_bytes_to_float(&frame[1]);
    std_msgs::msg::Float32 msg;
    msg.data = v;
    batt_pub_->publish(msg);
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 10000,
                         "battery %.2f V", v);
  }
}

/* ==================== 里程计 ==================== */

float Rock4DNode::be_bytes_to_float(const uint8_t * p)
{
  uint32_t u = (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) |
               static_cast<uint32_t>(p[3]);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

int32_t Rock4DNode::be_bytes_to_int32(const uint8_t * p)
{
  return (static_cast<int32_t>(p[0]) << 24) |
         (static_cast<int32_t>(p[1]) << 16) |
         (static_cast<int32_t>(p[2]) << 8) |
         static_cast<int32_t>(p[3]);
}

void Rock4DNode::publish_odom(float yaw_deg, int32_t pos_l, int32_t pos_r)
{
  double yaw_rad = yaw_deg * M_PI / 180.0;
  double dt = 1.0 / request_hz_;   /* 固定采样间隔: 1.0/50 = 0.02s */
  rclcpp::Time now = this->now();

  int32_t d_l = 0, d_r = 0;
  if (have_last_pos_) {
    d_l = (pos_l - last_pos_l_);
    d_r = (pos_r - last_pos_r_);
  }
  last_pos_l_ = pos_l;
  last_pos_r_ = pos_r;
  have_last_pos_ = true;

  /* 位移 = 脉冲差 × 每脉冲米数 */
  double dist_l = d_l * tick_to_m_;
  double dist_r = d_r * tick_to_m_;
  /* 速度 = 位移 / 时间 */
  double v_l = dist_l / dt;
  double v_r = dist_r / dt;

  double vx = (v_l + v_r) / 2.0;
  double omega = (v_r - v_l) / wheel_base_m_;

  /* 位移积分 */
  if (dt > 0.0 && dt < 0.5) {
    x_ += vx * std::cos(yaw_rad) * dt;
    y_ += vx * std::sin(yaw_rad) * dt;
  }
  yaw_ = yaw_rad;

  RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 20,
                       "odom: pos=(%d,%d) d=(%d,%d) vx=%.3f m/s",
                       pos_l, pos_r, d_l, d_r, vx);

  nav_msgs::msg::Odometry odom;
  odom.header.stamp = now;
  odom.header.frame_id = "odom";
  odom.child_frame_id = "base_link";
  odom.pose.pose.position.x = x_;
  odom.pose.pose.position.y = y_;
  tf2::Quaternion q;
  q.setRPY(0.0, 0.0, yaw_rad);
  odom.pose.pose.orientation.x = q.x();
  odom.pose.pose.orientation.y = q.y();
  odom.pose.pose.orientation.z = q.z();
  odom.pose.pose.orientation.w = q.w();
  odom.twist.twist.linear.x = vx;
  odom.twist.twist.angular.z = omega;
  odom_pub_->publish(odom);

  geometry_msgs::msg::TransformStamped t;
  t.header.stamp = now;
  t.header.frame_id = "odom";
  t.child_frame_id = "base_link";
  t.transform.translation.x = x_;
  t.transform.translation.y = y_;
  t.transform.translation.z = 0.0;
  t.transform.rotation = odom.pose.pose.orientation;
  tf_broadcaster_->sendTransform(t);
}

/* ==================== 回调 ==================== */

void Rock4DNode::poll_cb()
{
  drain_serial();       /* 先读: 上一次请求的响应(STM32已发完) */
  send_control(2, 1);   /* 再发新请求 */
}

void Rock4DNode::cmd_vel_cb(const geometry_msgs::msg::Twist::SharedPtr msg)
{
  RCLCPP_INFO(get_logger(), "RX cmd_vel v=%.3f w=%.3f", msg->linear.x, msg->angular.z);
  send_cmd_vel(msg->linear.x, msg->angular.z);
}

/* ==================== 程序入口 ==================== */

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Rock4DNode>());
  rclcpp::shutdown();
  return 0;
}
