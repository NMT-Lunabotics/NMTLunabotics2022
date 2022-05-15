#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <iostream>
#include <math.h>
#include <thread>

#include <actionlib/server/simple_action_server.h>
#include <david_action/PitchAction.h>
#include <move_base_msgs/MoveBaseGoal.h>
#include <ros/node_handle.h>
#include <ros/ros.h>
#include <sensor_msgs/JointState.h>

#include "david_action/PitchFeedback.h"
#include "gpio_lib/rpi_gpio.hpp"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::this_thread::sleep_for;

using namespace std;

enum class Goal {
  HOME = 0,
  EXTEND = 1,
  RETRACT = 2,
  HALF_EXTEND = 3,
};

// Robot constants

class PitchAction {
  ros::NodeHandle _nh;

  actionlib::SimpleActionServer<david_action::PitchAction> _as;

  ros::Publisher _joint_publisher;
  string _action_name;

  GPIOOut _home;
  GPIOOut _extend;
  GPIOOut _retract;
  GPIOOut _half_extend;

  void executeCB(const david_action::PitchGoalConstPtr &goal) {
    cout << "Entered ExecuteCB" << endl;

    // Do motor stuff
    GPIOOut *pin;
    switch ((Goal)goal->goal_state) {
    case Goal::HOME:
      pin = &_home;
      break;
    case Goal::EXTEND:
      pin = &_extend;
      break;
    case Goal::RETRACT:
      pin = &_retract;
      break;
    case Goal::HALF_EXTEND:
      pin = &_half_extend;
      break;
    default:
      cerr << "Warning: PitchAction: ignoring invalid goal state "
           << goal->goal_state << endl;
      return;
    }

    // Pulse the control pin quickly.
    pin->set(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pin->set(false);

    // Extension lengths in meters
    float extend_close = 0.46;
    float extend_half = 0.51;
    float extend_full = 0.675;
    float extend_length = -1;

    // Measured fixed sides
    float rear_pivot_2_fwd_hinge = 0.71;
    float fwd_hinge_2_mount_bracket = 0.39;

    // Set the appropriate extension length
    switch ((Goal)goal->goal_state) {
    case Goal::HOME:
      extend_length = extend_close;
      break;
    case Goal::EXTEND:
      extend_length = extend_full;
      break;
    case Goal::RETRACT:
      extend_length = extend_close;
      break;
    case Goal::HALF_EXTEND:
      extend_length = extend_half;
      break;
    default:
      cerr << "Warning: PitchAction: ignoring invalid goal state "
           << goal->goal_state << endl;
      return;
    }

    // Publish joint states
    // Abbreviate variable names
    float a = fwd_hinge_2_mount_bracket;
    float c = rear_pivot_2_fwd_hinge;

    // Closure for angle computation
    auto compute_angle = [a, c](double b) {
      return 3.14159 - acos((pow(a, 2) + pow(c, 2) - pow(b, 2)) / (2 * a * c));
    };

    // Joint angle
    float angle = compute_angle(extend_length);
    cout << extend_length << endl;

    // Construct a message
    sensor_msgs::JointState msg;
    msg.name = {"R_pitch"};
    msg.position = {angle};
    msg.velocity = {0};
    msg.effort = {0};

    // Wait for motors. On 2022-05-14 we measured that homing takes 47.79
    // seconds and extending takes 48.17 seconds, on 11.49 volts. The power
    // system uses 12 volts, therefore everything should be slightly faster, so
    // we should surely be done by 50 seconds.
    auto time_wait = seconds(50);

    // Publish feedback and wait
    david_action::PitchFeedback feedback;
    auto start = steady_clock::now();
    auto current = steady_clock::now();
    while ((current - start) < time_wait) {
      current = steady_clock::now();
      feedback.progress = (current - start) / time_wait;
      _as.publishFeedback(feedback);
      sleep_for(milliseconds(100));
    }

    // Publish the joint state
    _joint_publisher.publish(msg);

    cout << "Message published" << endl;
    // Report success.
    david_action::PitchResult result;
    _as.setSucceeded(result);
  }

public:
  PitchAction(string name)
      : _as(_nh, name, bind(&PitchAction::executeCB, this, _1), false),
        _action_name(name),
        // Pin numbers
        _home(21), _extend(20), _retract(12), _half_extend(16) {
    _joint_publisher =
        _nh.advertise<sensor_msgs::JointState>("/joints/pitch", 1000);

    _as.start();
  }
};

int main(int argc, char **argv) {
  ros::init(argc, argv, "pitch");

  PitchAction nav("pitch");
  ros::spin();
}
