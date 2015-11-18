/************************************************************/
/*    FILE: PololuApp.cpp
/*    ORGN: Toutatis AUVs - ENSTA Bretagne
/*    AUTH: Simon Rohou
/*    DATE: 2015
/************************************************************/

#include <iostream>
#include <fstream>

#include <sstream>
#include <iterator>
#include "math.h"
#include "MBUtils.h"
#include "ACTable.h"
#include "PololuApp.h"

using namespace std;

//---------------------------------------------------------
// Constructor

PololuApp::PololuApp()
{

}

//---------------------------------------------------------
// Destructor

PololuApp::~PololuApp()
{
  // Reset
  for(map<string,PololuPinOut*>::iterator it = m_map_pinouts.begin() ;
      it != m_map_pinouts.end() ;
      ++it)
  {
    if(!m_pololu->setTarget(it->second->getPinNumber(), it->second->getPwmZero() * 4))
      reportRunWarning("Error setting zero target: " + it->first);
  }

  m_map_pinouts.clear();
  delete m_pololu;
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool PololuApp::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin() ; p != NewMail.end() ; p++)
  {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();

    #if 0 // Keep these around just for template
      string comm  = msg.GetCommunity();
      double dval  = msg.GetDouble();
      string sval  = msg.GetString(); 
      string msrc  = msg.GetSource();
      double mtime = msg.GetTime();
      bool   mdbl  = msg.IsDouble();
      bool   mstr  = msg.IsString();
    #endif

    if(m_map_pinouts.count(toupper(key)) > 0)
    {
      m_map_pinouts[key]->setValue(msg.GetDouble());
      if(!m_pololu->setTarget(m_map_pinouts[key]->getPinNumber(), m_map_pinouts[key]->getPwmValue() * 4))
        reportRunWarning("Error setting target: " + key);
    }

    else if(key != "APPCAST_REQ") // handle by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);
  }

  return true;
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool PololuApp::OnConnectToServer()
{
  registerVariables();
  return true;
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool PololuApp::Iterate()
{
  AppCastingMOOSApp::Iterate();

  string error_message;
  bool pololu_ok = m_pololu->isReady(error_message);
  Notify("POLOLU_STATUS", pololu_ok ? "ok" : error_message);

  AppCastingMOOSApp::PostReport();
  return true;
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool PololuApp::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  int pin_number = -1;
  int pwm_mini = 1200;
  int pwm_zero = 1500;
  int pwm_maxi = 1800;
  bool bilateral_range = true;
  bool reverse = false;

  STRING_LIST::iterator p;
  sParams.reverse();
  for(p = sParams.begin() ; p != sParams.end() ; p++)
  {
    string orig  = *p;
    string line  = *p;
    string param = toupper(biteStringX(line, '='));
    string value = line;
    bool handled = false;

    if(param == "PIN_NUMBER")
    {
      pin_number = atoi(value.c_str());
      handled = true;
    }

    if(param == "BILATERAL_RANGE")
    {
      bilateral_range = value == "true";
      handled = true;
    }

    if(param == "REVERSE")
    {
      reverse = value == "true";
      handled = true;
    }

    if(param == "PWM_MINI")
    {
      pwm_mini = atoi(value.c_str());
      handled = true;
    }

    if(param == "PWM_ZERO")
    {
      pwm_zero = atoi(value.c_str());
      handled = true;
    }

    if(param == "PWM_MAXI")
    {
      pwm_maxi = atoi(value.c_str());
      handled = true;
    }

    if(param == "MOOSVAR_SUBSCRIPTION" && pin_number != -1)
    {
      PololuPinOut *new_pin = new PololuPinOut(pin_number);
      new_pin->setPwmMini(pwm_mini);
      new_pin->setPwmZero(pwm_zero);
      new_pin->setPwmMaxi(pwm_maxi);
      new_pin->setBilaterality(bilateral_range);
      new_pin->reverse(reverse);
      m_map_pinouts[toupper(value)] = new_pin;
      handled = true;
    }

    if(param == "DEVICE_NAME")
    {
      m_device_name = value;
      handled = true;
    }
    
    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  m_pololu = new Pololu(m_device_name);

  if(!m_pololu->isReady())
    reportRunWarning("Error on Pololu connection.");

  return true;
}

//---------------------------------------------------------
// Procedure: registerVariables

void PololuApp::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  for(map<string,PololuPinOut*>::iterator it = m_map_pinouts.begin() ;
      it != m_map_pinouts.end() ;
      ++it)
  {
    Register(it->first, 0);
  }
}

//------------------------------------------------------------
// Procedure: buildReport()

bool PololuApp::buildReport() 
{
  string error_message;
  bool pololu_ok = m_pololu->isReady(error_message);
  m_msgs << "Pololu status: \t" << (pololu_ok ? "ok" : error_message) << "\n";
  m_msgs << "\n";

  ACTable actab_pwm(8);
  actab_pwm << "MoosVar" << "Pin" << "Value" << "Mini" << "Zero" << "Maxi" << "Reversed" << "Bilateral";
  actab_pwm.addHeaderLines();
  for(map<string,PololuPinOut*>::iterator it = m_map_pinouts.begin() ;
        it != m_map_pinouts.end() ;
        ++it)
  {
    actab_pwm << it->first 
              << it->second->getPinNumber() 
              << it->second->getPwmValue()
              << it->second->getPwmMini()
              << it->second->getPwmZero()
              << it->second->getPwmMaxi()
              << (it->second->isReversed() ? "yes" : "no")
              << (it->second->isBilateral() ? "yes" : "no");
  }
  m_msgs << actab_pwm.getFormattedString() << "\n\n";
  
  return true;
}