// Interface to talking to Teknic motors.

// Copyright (c) 2022 NMT Lunabotics. All rights reserved.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <pubSysCls.h>
#include <ros/ros.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "main.hpp"
#include "teknic.hpp"
#include "teknic/motor_utils.hpp"

using namespace sFnd;
using namespace std;

// Construct a motor controller at the given path, and initialize it
// to zero velocity.
TeknicMotor::TeknicMotor(SimpleNode &node) : _node(node) {
  _manager = thread([this]() { this->motor_manager(); });
}

void TeknicMotor::setVelocity(double vel) { _vel_target = vel; }

void TeknicMotor::setPosition(double pos) {
  // _node.setPos(pos);
  // That might RMS out.
  cerr << "TeknicMotor::setPosition unimplemented" << endl;
  abort();
}

// Get motor position (returns encoder count).
double TeknicMotor::position(){
  return _node.position();
}

// Get motor velocity (returns RPM).
double TeknicMotor::velocity(){
  return _node.velocity();
}

// Get measured torque (returns percentage of maximum by default).
double TeknicMotor::torque(){
  return _node.torque();
}

// Get measured rms_level (returns percentage of maximum).
double TeknicMotor::rms(){
  return _node.rms();
}

// Initializes the navigation motors for the robot.
vector<NavMotor> init_motors(string path, NavMotor *&augerMotor,
                             NavMotor *&depthLMotor, NavMotor *&depthRMotor,
                             NavMotor *&dumperLMotor, NavMotor *&dumperRMotor) {
  vector<SimplePort> ports = SimplePort::getPorts();

  // This leaks `ports[0]` on purpose, because the `SimplePort` has to
  // be alive for the `SimpleNode`s to function. Ideally we would
  // change the return type here to be some type that keeps the
  // `SimplePort` alive, but I don't have time.
  SimplePort *port = new SimplePort(move(ports[0]));

  vector<SimpleNode> &nodes = port->getNodes();
  vector<NavMotor> motors;

  // And this leaks TeknicMotor objects. Cry about it.
  motors.push_back(
                   NavMotor(new TeknicMotor(nodes.at(MotorIdent::LocomotionL)), "loco_left", -1, -1));
  motors.push_back(
                   NavMotor(new TeknicMotor(nodes.at(MotorIdent::LocomotionR)), "loco_right", 1, 1));

  augerMotor = new NavMotor(new TeknicMotor(nodes.at(MotorIdent::Auger)), "auger_rotation", 0, 1);
  depthLMotor =
    new NavMotor(new TeknicMotor(nodes.at(MotorIdent::DepthL)), "L_depth", 0, 1);
  depthRMotor =
    new NavMotor(new TeknicMotor(nodes.at(MotorIdent::DepthR)),"R_depth" , 0, 1);
  dumperLMotor =
    new NavMotor(new TeknicMotor(nodes.at(MotorIdent::DumpL)), "left_dump" ,0, 1);
  dumperRMotor =
    new NavMotor(new TeknicMotor(nodes.at(MotorIdent::DumpR)),"right_dump", 0, 1);

  return motors;
}

void TeknicMotor::motor_manager() {
  while (true) {
    // double rms = _node.rms();
    // double max_rms = 1;

    // // Use epsilon ratios of 1000 rpm to serve as the assumed maximum
    // // RPM, if we're not moving (i.e., _vel_current and rms are both
    // // zero).
    // double rpm_per_rms = (_vel_current + 0.001) / (rms + 0.000001);
    // double max_rpm = max_rms * rpm_per_rms;

    // _vel_current = min((double)_vel_target, max_rpm);
    // _node.setVel(_vel_current);

    // that's not working, and I'm too tired to figure out why.
    _node.setVel(_vel_target);

    std::this_thread::sleep_for(
        std::chrono::microseconds(1000000 / MANAGER_RESOLUTION));
  }
}
