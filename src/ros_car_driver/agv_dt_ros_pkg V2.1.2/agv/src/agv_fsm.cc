
#include <string>

#include "agv_tinyfsm.hpp"
#include "agv_fsm.h"

using FsmPort = fsm::Port::Fsm;
using FsmPortEvent = fsm::Port::Event;
using FsmPortState = fsm::Port::State;

std::string FsmPort::now_state_ = "";
std::string FsmPort::getNowState(void) {return FsmPort::now_state_;}
void FsmPort::setNowState(std::string state) {FsmPort::now_state_ = state;}
void FsmPort::entry(void) {}
void FsmPort::exit(void) {}
void FsmPort::react(const FsmPortEvent::Init &event) {}
void FsmPort::react(const FsmPortEvent::InitFail &event) {}
void FsmPort::react(const FsmPortEvent::InitSuccess &event) {}
void FsmPort::react(const FsmPortEvent::Open &event) {}
void FsmPort::react(const FsmPortEvent::OpenFail &event) {}
void FsmPort::react(const FsmPortEvent::OpenSuccess &event) {}
void FsmPort::react(const FsmPortEvent::Idle &event) {}
void FsmPort::react(const FsmPortEvent::Link &event) {}
void FsmPort::react(const FsmPortEvent::LinkFail &event) {}
void FsmPort::react(const FsmPortEvent::LinkSuccess &event) {}
void FsmPort::react(const FsmPortEvent::Read &event) {}
void FsmPort::react(const FsmPortEvent::ReadFail &event) {}
void FsmPort::react(const FsmPortEvent::ReadSuccess &event) {}
void FsmPort::react(const FsmPortEvent::Write &event) {}
void FsmPort::react(const FsmPortEvent::WriteFail &event) {}
void FsmPort::react(const FsmPortEvent::WriteSuccess &event) {}
void FsmPort::react(const FsmPortEvent::Restart &event) {}
void FsmPort::react(const FsmPortEvent::Close &event) {}
void FsmPort::react(tinyfsm::Event const &) {}
FSM_INITIAL_STATE(FsmPort, FsmPortState::Close)

void FsmPortState::Close::entry(void) {setNowState(name);}
void FsmPortState::Close::react(const FsmPortEvent::Init &event) {transit<FsmPortState::Init>();}

void FsmPortState::Init::entry(void) {setNowState(name);}
void FsmPortState::Init::react(const FsmPortEvent::InitFail &event) {transit<FsmPortState::InitFail>();}
void FsmPortState::Init::react(const FsmPortEvent::InitSuccess &event) {transit<FsmPortState::InitSuccess>();}
void FsmPortState::InitFail::entry(void) {setNowState(name);}
void FsmPortState::InitFail::react(const FsmPortEvent::Restart &event) {transit<FsmPortState::Restart>();}
void FsmPortState::InitSuccess::entry(void) {setNowState(name);}
void FsmPortState::InitSuccess::react(const FsmPortEvent::Open &event) {transit<FsmPortState::Open>();}

void FsmPortState::Open::entry(void) {setNowState(name);}
void FsmPortState::Open::react(const FsmPortEvent::OpenFail &event) {transit<FsmPortState::OpenFail>();}
void FsmPortState::Open::react(const FsmPortEvent::OpenSuccess &event) {transit<FsmPortState::OpenSuccess>();}
void FsmPortState::OpenFail::entry(void) {setNowState(name);}
void FsmPortState::OpenFail::react(const FsmPortEvent::Restart &event) {transit<FsmPortState::Restart>();}
void FsmPortState::OpenSuccess::entry(void) {setNowState(name);}
void FsmPortState::OpenSuccess::react(const FsmPortEvent::Idle &event) {transit<FsmPortState::Idle>();}

void FsmPortState::Idle::entry(void) {setNowState(name);}
void FsmPortState::Idle::react(const FsmPortEvent::Link &event) {transit<FsmPortState::Link>();}
void FsmPortState::Idle::react(const FsmPortEvent::Read &event) {transit<FsmPortState::Read>();}
void FsmPortState::Idle::react(const FsmPortEvent::Write &event) {transit<FsmPortState::Write>();}

void FsmPortState::Link::entry(void) {setNowState(name);}
void FsmPortState::Link::react(const FsmPortEvent::LinkFail &event) {transit<FsmPortState::LinkFail>();}
void FsmPortState::Link::react(const FsmPortEvent::LinkSuccess &event) {transit<FsmPortState::LinkSuccess>();}
void FsmPortState::LinkFail::entry(void) {setNowState(name);}
void FsmPortState::LinkFail::react(const FsmPortEvent::Restart &event) {transit<FsmPortState::Restart>();}
void FsmPortState::LinkSuccess::entry(void) {setNowState(name);}
void FsmPortState::LinkSuccess::react(const FsmPortEvent::Read &event) {transit<FsmPortState::Read>();}
void FsmPortState::LinkSuccess::react(const FsmPortEvent::Write &event) {transit<FsmPortState::Write>();}

void FsmPortState::Read::entry(void) {setNowState(name);}
void FsmPortState::Read::react(const FsmPortEvent::ReadFail &event) {transit<FsmPortState::ReadFail>();}
void FsmPortState::Read::react(const FsmPortEvent::ReadSuccess &event) {transit<FsmPortState::ReadSuccess>();}
void FsmPortState::ReadFail::entry(void) {setNowState(name);}
void FsmPortState::ReadFail::react(const FsmPortEvent::Restart &event) {transit<FsmPortState::Restart>();}
void FsmPortState::ReadSuccess::entry(void) {setNowState(name);}
void FsmPortState::ReadSuccess::react(const FsmPortEvent::Idle &event) {transit<FsmPortState::Idle>();}

void FsmPortState::Write::entry(void) {setNowState(name);}
void FsmPortState::Write::react(const FsmPortEvent::WriteFail &event) {transit<FsmPortState::WriteFail>();}
void FsmPortState::Write::react(const FsmPortEvent::WriteSuccess &event) {transit<FsmPortState::WriteSuccess>();}
void FsmPortState::WriteFail::entry(void) {setNowState(name);}
void FsmPortState::WriteFail::react(const FsmPortEvent::Restart &event) {transit<FsmPortState::Restart>();}
void FsmPortState::WriteSuccess::entry(void) {setNowState(name);}
void FsmPortState::WriteSuccess::react(const FsmPortEvent::Idle &event) {transit<FsmPortState::Idle>();}

void FsmPortState::Restart::entry(void) {setNowState(name);}
void FsmPortState::Restart::react(const FsmPortEvent::Close &event) {transit<FsmPortState::Close>();}

using FsmPackage = fsm::Package::Fsm;
using FsmPackageEvent = fsm::Package::Event;
using FsmPackageState = fsm::Package::State;

std::string FsmPackage::now_state_ = "";
std::string FsmPackage::getNowState(void) {return FsmPackage::now_state_;}
void FsmPackage::setNowState(std::string state) {FsmPackage::now_state_ = state;}
void FsmPackage::entry(void) {}
void FsmPackage::exit(void) {}
void FsmPackage::react(const FsmPackageEvent::Begin &event) {}
void FsmPackage::react(const FsmPackageEvent::Init &event) {}
void FsmPackage::react(const FsmPackageEvent::Idle &event) {}
void FsmPackage::react(const FsmPackageEvent::ReadDrive &event) {}
void FsmPackage::react(const FsmPackageEvent::PackageCheck &event) {}
void FsmPackage::react(const FsmPackageEvent::PackageExist &event) {}
void FsmPackage::react(const FsmPackageEvent::PackageNotExist &event) {}
void FsmPackage::react(const FsmPackageEvent::Restart &event) {}
void FsmPackage::react(tinyfsm::Event const &) {}
FSM_INITIAL_STATE(FsmPackage, FsmPackageState::Begin)

void FsmPackageState::Begin::entry(void) {setNowState(name);}
void FsmPackageState::Begin::react(const FsmPackageEvent::Init &event) {transit<FsmPackageState::Init>();}

void FsmPackageState::Init::entry(void) {setNowState(name);}
void FsmPackageState::Init::react(const FsmPackageEvent::Idle &event) {transit<FsmPackageState::Idle>();}

void FsmPackageState::Idle::entry(void) {setNowState(name);}
void FsmPackageState::Idle::react(const FsmPackageEvent::ReadDrive &event) {transit<FsmPackageState::ReadDrive>();}

void FsmPackageState::ReadDrive::entry(void) {setNowState(name);}
void FsmPackageState::ReadDrive::react(const FsmPackageEvent::PackageCheck &event) {transit<FsmPackageState::PackageCheck>();}

void FsmPackageState::PackageCheck::entry(void) {setNowState(name);}
void FsmPackageState::PackageCheck::react(const FsmPackageEvent::PackageExist &event) {transit<FsmPackageState::Idle>();}
void FsmPackageState::PackageCheck::react(const FsmPackageEvent::PackageNotExist &event) {transit<FsmPackageState::PackageTimeoutCheck>();}

void FsmPackageState::PackageTimeoutCheck::entry(void) {setNowState(name);}
void FsmPackageState::PackageTimeoutCheck::react(const FsmPackageEvent::Idle &event) {transit<FsmPackageState::Idle>();}
void FsmPackageState::PackageTimeoutCheck::react(const FsmPackageEvent::Restart &event) {transit<FsmPackageState::Restart>();}

void FsmPackageState::Restart::entry(void) {setNowState(name);}
void FsmPackageState::Restart::react(const FsmPackageEvent::Begin &event) {transit<FsmPackageState::Begin>();}
