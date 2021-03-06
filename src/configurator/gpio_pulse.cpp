/**
 * Copyright (C) 2017, Raffaello Bonghi <raffaello@rnext.it>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "configurator/gpio_pulse.h"

#define PARAM_PULSE_STRING "/pulse"

GPIOPulseConfigurator::GPIOPulseConfigurator(const ros::NodeHandle &nh, roboteq::serial_controller *serial, std::vector<roboteq::Motor *> motor, string name, unsigned int number)
    : nh_(nh)
    , mSerial(serial)
    ,_motor(motor)
{
    // Find path param
    mName = nh_.getNamespace() + name + PARAM_PULSE_STRING + "/" + std::to_string(number);
    // Roboteq motor number
    mNumber = number;
    // Set false on first run
    setup_param = false;
}

void GPIOPulseConfigurator::initConfigurator(bool load_from_board)
{
    // Check if is required load paramers
    if(load_from_board)
    {
        // Load parameters from roboteq
        getParamFromRoboteq();
    }
    // Initialize parameter dynamic reconfigure
    ds_param = new dynamic_reconfigure::Server<roboteq_control::RoboteqPulseInputConfig>(ros::NodeHandle(mName));
    dynamic_reconfigure::Server<roboteq_control::RoboteqPulseInputConfig>::CallbackType cb_param = boost::bind(&GPIOPulseConfigurator::reconfigureCBParam, this, _1, _2);
    ds_param->setCallback(cb_param);
}

void GPIOPulseConfigurator::getParamFromRoboteq()
{
    try
    {
        // conversion PMOD [pag. 302]
        string str_conversion = mSerial->getParam("PMOD", std::to_string(mNumber));
        int conversion = boost::lexical_cast<int>(str_conversion);
        // Set params
        nh_.setParam(mName + "/conversion", conversion);

        // input PINA [pag. 287]
        string str_pina = mSerial->getParam("PINA", std::to_string(mNumber));
        // Get PINA from roboteq board
        int emod = boost::lexical_cast<unsigned int>(str_pina);
        // 3 modes:
        // 0 - Unsed
        // 1 - Command
        // 2 - Feedback
        int command = (emod & 0b11);
        int motors = (emod - command) >> 4;
        int tmp1 = ((motors & 0b1) > 0);
        int tmp2 = ((motors & 0b10) > 0);
        if(tmp1)
        {
            for (vector<roboteq::Motor*>::iterator it = _motor.begin() ; it != _motor.end(); ++it)
            {
                roboteq::Motor* motor = ((roboteq::Motor*)(*it));
                if(motor->getNumber() == 1)
                {
                    motor->registerSensor(this);
                    ROS_INFO_STREAM("Register pulse input [" << mNumber << "] to: " << motor->getName());
                    break;
                }
            }
        }
        if(tmp2)
        {
            for (vector<roboteq::Motor*>::iterator it = _motor.begin() ; it != _motor.end(); ++it)
            {
                roboteq::Motor* motor = ((roboteq::Motor*)(*it));
                if(motor->getNumber() == 2)
                {
                    motor->registerSensor(this);
                    ROS_INFO_STREAM("Register pulse input [" << mNumber << "] to: " << motor->getName());
                    break;
                }
            }
        }

        // Set parameter
        nh_.setParam(mName + "/input_use", command);
        nh_.setParam(mName + "/input_motor_one", tmp1);
        nh_.setParam(mName + "/input_motor_two", tmp2);

        // polarity PPOL [pag. 303]
        string str_polarity = mSerial->getParam("PPOL", std::to_string(mNumber));
        int polarity = boost::lexical_cast<int>(str_polarity);
        // Set params
        nh_.setParam(mName + "/conversion_polarity", polarity);

        // Input deadband PDB [pag. 297]
        string str_deadband = mSerial->getParam("PDB", std::to_string(mNumber));
        int deadband = boost::lexical_cast<int>(str_deadband);
        // Set params
        nh_.setParam(mName + "/input_deadband", deadband);

        // Input PMIN [pag. 301]
        string str_min = mSerial->getParam("PMIN", std::to_string(mNumber));
        double min = boost::lexical_cast<double>(str_min) / 1000;
        // Set params
        nh_.setParam(mName + "/range_input_min", min);

        // Input PMAX [pag. 300]
        string str_max = mSerial->getParam("PMAX", std::to_string(mNumber));
        double max = boost::lexical_cast<double>(str_max) / 1000;
        // Set params
        nh_.setParam(mName + "/range_input_max", max);

        // Input PTCR [pag. 293]
        string str_ctr = mSerial->getParam("PCTR", std::to_string(mNumber));
        double ctr = boost::lexical_cast<double>(str_ctr) / 1000;
        // Set params
        nh_.setParam(mName + "/range_input_center", ctr);

    } catch (std::bad_cast& e)
    {
        ROS_WARN_STREAM("Failure parsing feedback data. Dropping message." << e.what());
    }
}

void GPIOPulseConfigurator::reconfigureCBParam(roboteq_control::RoboteqPulseInputConfig &config, uint32_t level)
{
    //The first time we're called, we just want to make sure we have the
    //original configuration
    if(!setup_param)
    {
      _last_param_config = config;
      default_param_config = _last_param_config;
      setup_param = true;
      return;
    }

    if(config.restore_defaults)
    {
        //if someone sets restore defaults on the parameter server, prevent looping
        config.restore_defaults = false;
        // Overload config with default
        config = default_param_config;
    }

    if(config.load_roboteq)
    {
        //if someone sets again the request on the parameter server, prevent looping
        config.load_roboteq = false;
        // Launch param load
        getParamFromRoboteq();
        // Skip other read
        return;
    }

    // Set conversion PMOD [pag. 302]
    if(_last_param_config.conversion != config.conversion)
    {
        // Update operative mode
        mSerial->setParam("PMOD", std::to_string(mNumber) + " " + std::to_string(config.conversion));
    }
    // Set input PINA [pag. 287]
    if((_last_param_config.input_use != config.input_use) ||
            (_last_param_config.input_motor_one != config.input_motor_one) ||
            (_last_param_config.input_motor_two != config.input_motor_two))
    {
        int input = config.input_use + 16*config.input_motor_one + 32*config.input_motor_two;
        mSerial->setParam("PINA", std::to_string(mNumber) + " " + std::to_string(input));

        if(config.input_motor_one)
        {
            roboteq::Motor* motor = _motor.at(0);
            motor->registerSensor(this);
            ROS_INFO_STREAM("Register pulse input [" << mNumber << "] to: " << motor->getName());
        }
        if(config.input_motor_two)
        {
            roboteq::Motor* motor = _motor.at(1);
            motor->registerSensor(this);
            ROS_INFO_STREAM("Register pulse input [" << mNumber << "] to: " << motor->getName());
        }
    }
    // Set polarity PPOL [pag. 303]
    if(_last_param_config.conversion_polarity != config.conversion_polarity)
    {
        // Update operative mode
        mSerial->setParam("PPOL", std::to_string(mNumber) + " " + std::to_string(config.conversion_polarity));
    }
    // Set deadband PDB [pag. 297]
    if(_last_param_config.input_deadband != config.input_deadband)
    {
        // Update operative mode
        mSerial->setParam("PDB", std::to_string(mNumber) + " " + std::to_string(config.input_deadband));
    }
    // Set input PMIN [pag. 301]
    if(_last_param_config.range_input_min != config.range_input_min)
    {
        int range_input_min = config.range_input_min * 1000;
        // Update operative mode
        mSerial->setParam("PMIN", std::to_string(mNumber) + " " + std::to_string(range_input_min));
    }
    // Set input PMAX [pag. 300]
    if(_last_param_config.range_input_max != config.range_input_max)
    {
        int range_input_max = config.range_input_max * 1000;
        // Update operative mode
        mSerial->setParam("PMAX", std::to_string(mNumber) + " " + std::to_string(range_input_max));
    }
    // Set input PTCR [pag. 293]
    if(_last_param_config.range_input_center != config.range_input_center)
    {
        int range_input_center = config.range_input_center * 1000;
        // Update operative mode
        mSerial->setParam("PCTR", std::to_string(mNumber) + " " + std::to_string(range_input_center));
    }
}
