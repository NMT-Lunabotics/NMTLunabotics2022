// ROS controller.

// Copyright (c) 2009-2021 Josh Faust et al., 2021 NMT Lunabotics.
// All rights reserved.

#include <ros/ros.h>
#include <webots_ros/set_float.h>

#include <iostream>
#include <limits>
#include <string>
#include <utility>

#ifndef _WIN32
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "controller.hpp"

#define KEYCODE_RIGHT 0x43
#define KEYCODE_LEFT 0x44
#define KEYCODE_UP 0x41
#define KEYCODE_DOWN 0x42
#define KEYCODE_B 0x62
#define KEYCODE_C 0x63
#define KEYCODE_D 0x64
#define KEYCODE_E 0x65
#define KEYCODE_F 0x66
#define KEYCODE_G 0x67
#define KEYCODE_Q 0x71
#define KEYCODE_R 0x72
#define KEYCODE_T 0x74
#define KEYCODE_V 0x76

using namespace std;

KeyboardReader::KeyboardReader()
#ifndef _WIN32
    : kfd(0)
#endif
{
#ifndef _WIN32
  // get the console in raw mode
  tcgetattr(kfd, &cooked);
  struct termios raw;
  memcpy(&raw, &cooked, sizeof(struct termios));
  raw.c_lflag &= ~(ICANON | ECHO);
  // Setting a new line, then end of file
  raw.c_cc[VEOL] = 1;
  raw.c_cc[VEOF] = 2;
  tcsetattr(kfd, TCSANOW, &raw);
#endif
}

void KeyboardReader::readOne(char *c) {
#ifndef _WIN32
  int rc = read(kfd, c, 1);
  if (rc < 0) {
    throw std::runtime_error("read failed");
  }
#else
  for (;;) {
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD buffer;
    DWORD events;
    PeekConsoleInput(handle, &buffer, 1, &events);
    if (events > 0) {
      ReadConsoleInput(handle, &buffer, 1, &events);
      if (buffer.Event.KeyEvent.wVirtualKeyCode == VK_LEFT) {
        *c = KEYCODE_LEFT;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == VK_UP) {
        *c = KEYCODE_UP;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == VK_RIGHT) {
        *c = KEYCODE_RIGHT;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == VK_DOWN) {
        *c = KEYCODE_DOWN;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x42) {
        *c = KEYCODE_B;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x43) {
        *c = KEYCODE_C;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x44) {
        *c = KEYCODE_D;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x45) {
        *c = KEYCODE_E;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x46) {
        *c = KEYCODE_F;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x47) {
        *c = KEYCODE_G;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x51) {
        *c = KEYCODE_Q;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x52) {
        *c = KEYCODE_R;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x54) {
        *c = KEYCODE_T;
        return;
      } else if (buffer.Event.KeyEvent.wVirtualKeyCode == 0x56) {
        *c = KEYCODE_V;
        return;
      }
    }
  }
#endif
}

void KeyboardReader::shutdown() {
#ifndef _WIN32
  tcsetattr(kfd, TCSANOW, &cooked);
#endif
}

KeyboardReader input;


Motor::Motor(string path) : pos_(0), vel_(0) {
  pos_path_ = path + "/set_position";
  vel_path_ = path + "/set_velocity";

  update();
}

void Motor::update() {
  webots_ros::set_float srv_pos;
  srv_pos.request.value = pos_;
  ros::service::call(pos_path_, srv_pos);

  webots_ros::set_float srv_vel;
  srv_vel.request.value = vel_;
  ros::service::call(vel_path_, srv_vel);
}

void Motor::setVelocity(double vel) {
  pos_ = std::numeric_limits<double>::infinity();
  vel_ = vel;

  update();
}

void Motor::setPosition(double pos) {
  pos_ = pos;
  // The value of `vel_' will be ignored anyway, don't bother fiddling
  // with it.

  update();
}


NavMotor::NavMotor(Motor m, pair<double, double> mot_vec)
    : motor_(m), mot_vec_(mot_vec) {}

void NavMotor::nav(pair<double, double> nav_vec) {
  auto dot = mot_vec_.first * nav_vec.first + mot_vec_.second * nav_vec.second;
  motor_.setVelocity(dot);
}


TeleopLunabotics::TeleopLunabotics(string path)
    : back_left_(Motor(path + "/motor2"), pair(1.0, 0.0)),
      back_right_(Motor(path + "/motor4"), pair(0.0, 1.0)),
      front_left_(Motor(path + "/motor1"), pair(1.0, 0.0)),
      front_right_(Motor(path + "/motor3"), pair(0.0, 1.0)), robot_path_(path) {
}


void quit(int sig) {
  (void)sig;
  input.shutdown();
  ros::shutdown();
  exit(0);
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "teleop_turtle");

  if (argc != 2) {
    cout << "Usage: " << argv[0] << " <robot-path>" << endl;
    return 1;
  }

  string robot_path(argv[1]);
  TeleopLunabotics teleop(robot_path);

  signal(SIGINT, quit);

  teleop.keyLoop();
  quit(0);
}

void TeleopLunabotics::keyLoop() {
  char c;
  auto nav = pair(0.0, 0.0);

  puts("Reading from keyboard");
  puts("---------------------------");
  puts("Use arrow keys to move the robot. 'q' to quit.");

  while (1) {
    // get the next event from the keyboard
    try {
      input.readOne(&c);
    } catch (const std::runtime_error &) {
      perror("read():");
      return;
    }

    switch (c) {
    case KEYCODE_LEFT:
      ROS_DEBUG("LEFT");
      nav = pair(-1.0, 1.0);
      break;
    case KEYCODE_RIGHT:
      ROS_DEBUG("RIGHT");
      nav = pair(1.0, -1.0);
      break;
    case KEYCODE_UP:
      ROS_DEBUG("UP");
      nav = pair(1.0, 1.0);
      break;
    case KEYCODE_DOWN:
      ROS_DEBUG("DOWN");
      nav = pair(-1.0, -1.0);
      break;
    case KEYCODE_Q:
      ROS_DEBUG("quit");
      return;
    }

    back_left_.nav(nav);
    back_right_.nav(nav);
    front_left_.nav(nav);
    front_right_.nav(nav);
  }

  return;
}
