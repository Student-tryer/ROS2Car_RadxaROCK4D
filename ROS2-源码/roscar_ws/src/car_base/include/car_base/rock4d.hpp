#pragma once

#include <cstddef>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_msgs/msg/float32.hpp>
#include <tf2_ros/transform_broadcaster.h>

#include <string>
#include <vector>
#include <mutex>

/* Rock 4D 端底盘驱动节点
 *
 * 串口协议 (与 STM32 约定, 浮点全大端):
 *   下行 cmd_vel : AA 55 | v(float BE) | w(float BE) | 0D 0A     (12字节)
 *   下行 控制    : AA 55 | id | data | 0D 0A                     (6字节)
 *                   id=1: 0停/1启   id=2: 0复位/1请求数据   id=3: 0断开/1连接
 *   上行 odometry: A1 | yaw(float BE) | pos1(int32 BE) | pos2(int32 BE) | B1  (14字节)
 *   上行 电池     : A2 | v(float BE) | B2   (6字节)
 *
 * pos1/pos2 是 STM32 累计编码器脉冲(int32), 本节点用相邻两次差值算位移/速度。
 * yaw 来自 IMU(度)。
 */
class Rock4DNode : public rclcpp::Node
{
public:
  Rock4DNode();

private:
  /* 串口 */
  int serial_fd_ = -1;
  std::string port_;
  int baud_;
  std::mutex serial_mutex_;   /* 串口并发写锁 */

  /* 底盘参数 */
  double wheel_base_m_;
  double wheel_circ_m_;
  int encoder_ppr_;
  int loop_hz_;
  int request_hz_;
  double tick_to_m_;

  /* 里程计状态 */
  double x_ = 0.0;
  double y_ = 0.0;
  double yaw_ = 0.0;
  rclcpp::Time last_time_;
  int32_t last_pos_l_ = 0;
  int32_t last_pos_r_ = 0;
  bool have_last_pos_ = false;

  /* 发布/订阅 */
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr batt_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  std::shared_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  rclcpp::TimerBase::SharedPtr poll_timer_;

  /* 接收解析缓冲 (A1帧14字节: 头+12+B1尾; A2帧6字节: 头+4+B2尾) */
  uint8_t rx_frame_[14];
  size_t rx_frame_len_ = 0;
  size_t rx_frame_expected_ = 0;   /* 当前帧期望总长, 0=未在收帧 */

  /* 串口底层 */
  bool open_serial();
  ssize_t write_bytes(const uint8_t * buf, size_t len);
  void drain_serial();

  /* 协议 */
  void send_cmd_vel(double v, double w);
  void send_control(uint8_t id, uint8_t data);
  void on_rx_byte(uint8_t b);
  void parse_frame(const uint8_t * frame, size_t len);

  /* 回调 */
  void poll_cb();
  void cmd_vel_cb(const geometry_msgs::msg::Twist::SharedPtr msg);

  /* 工具 */
  static float be_bytes_to_float(const uint8_t * p);
  static int32_t be_bytes_to_int32(const uint8_t * p);
  void publish_odom(float yaw_deg, int32_t pos_l, int32_t pos_r);
};