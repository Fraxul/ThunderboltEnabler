#include "IOThunderboltICM.h"

OSDefineMetaClassAndStructors(IOThunderboltICM, OSObject);

void IOThunderboltICM::free() {

}

void IOThunderboltICM::createResources(void) {
}

void IOThunderboltICM::destroyResources(void) {
}

void IOThunderboltICM::configurationThreadDone(void) {
}

void IOThunderboltICM::configurationThreadMain(void) {
}

void IOThunderboltICM::resetRootSwitch(void) {
}

void IOThunderboltICM::scanRootSwitch(unsigned int) {
}

void IOThunderboltICM::scan(void *) {
}

void IOThunderboltICM::earlyWakeScan(void *) {
}

void IOThunderboltICM::wakeScan(void *) {
}

void IOThunderboltICM::rescan(void *) {
}

bool IOThunderboltICM::initWithController(IOThunderboltController* controller_) {
  if (!init())
    return false;

  m_controller = controller_;

  return true;
}

IOThunderboltSwitch* IOThunderboltICM::getRootSwitch(void) {
  return NULL;
}

IOThunderboltPort* IOThunderboltICM::getNHIPort(void) {
  return NULL;
}

bool IOThunderboltICM::isConfigurationThreadRunning(void) {
  return false;
}

void IOThunderboltICM::startConfigurationThread(IOThunderboltICM::Callback) {
}

void IOThunderboltICM::startScan(void) {
}

void IOThunderboltICM::startEarlyWakeScan(void) {
}

void IOThunderboltICM::startWakeScan(void) {
}

void IOThunderboltICM::lateSleep(void) {
}

void IOThunderboltICM::lateSleepPhase2(void) {
}

void IOThunderboltICM::startRescan(void) {
}

void IOThunderboltICM::appendSwitchToScanQueue(IOThunderboltSwitch *) {
}

void IOThunderboltICM::resetAll(void) {
}

void IOThunderboltICM::getDevicesConnectedState(bool *) {
}

void IOThunderboltICM::createRootSwitch(void) {
}

void IOThunderboltICM::terminateAndRegister(void) {
}

void IOThunderboltICM::disableConfigurationThread(void) {
}

void IOThunderboltICM::enableConfigurationThread(void) {
}


void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager0(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager1(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager2(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager3(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager4(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager5(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager6(void) {
}

void IOThunderboltICM::_RESERVEDIOThunderboltConnectionManager7(void) {
}


