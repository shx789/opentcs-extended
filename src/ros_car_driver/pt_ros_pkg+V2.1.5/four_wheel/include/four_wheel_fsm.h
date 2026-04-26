
#ifndef FSM_H
#define FSM_H

#include <string>

#include "four_wheel_tinyfsm.hpp"

namespace fsm
{

struct Port
{
  struct Event
  {
    struct Close : tinyfsm::Event {};

    struct Init        : tinyfsm::Event {};
    struct InitFail    : tinyfsm::Event {};
    struct InitSuccess : tinyfsm::Event {};

    struct Open        : tinyfsm::Event {};
    struct OpenFail    : tinyfsm::Event {};
    struct OpenSuccess : tinyfsm::Event {};

    struct Idle : tinyfsm::Event {};

    struct Link        : tinyfsm::Event {};
    struct LinkFail    : tinyfsm::Event {};
    struct LinkSuccess : tinyfsm::Event {};

    struct Read        : tinyfsm::Event {};
    struct ReadFail    : tinyfsm::Event {};
    struct ReadSuccess : tinyfsm::Event {};

    struct Write        : tinyfsm::Event {};
    struct WriteFail    : tinyfsm::Event {};
    struct WriteSuccess : tinyfsm::Event {};

    struct Restart : tinyfsm::Event {};
  };

  class Fsm : public tinyfsm::Fsm<Fsm>
  {
    public:

      std::string getNowState(void);

      virtual void entry(void);
      virtual void exit(void);

      virtual void react(const Event::Init &event);
      virtual void react(const Event::InitFail &event);
      virtual void react(const Event::InitSuccess &event);

      virtual void react(const Event::Open &event);
      virtual void react(const Event::OpenFail &event);
      virtual void react(const Event::OpenSuccess &event);

      virtual void react(const Event::Idle &event);

      virtual void react(const Event::Link &event);
      virtual void react(const Event::LinkFail &event);
      virtual void react(const Event::LinkSuccess &event);

      virtual void react(const Event::Read &event);
      virtual void react(const Event::ReadFail &event);
      virtual void react(const Event::ReadSuccess &event);

      virtual void react(const Event::Write &event);
      virtual void react(const Event::WriteFail &event);
      virtual void react(const Event::WriteSuccess &event);

      virtual void react(const Event::Restart &event);
      virtual void react(const Event::Close &event);

      void react(const tinyfsm::Event&);

    protected:

      void setNowState(std::string state);

    private:

      static std::string now_state_;
  };

  struct State
  {
    struct Close : public Fsm
    {
      const std::string name = "close";
      void entry(void) override;
      void react(const Event::Init &event) override;
    };

    struct Init : public Fsm
    {
      const std::string name = "init";
      void entry(void) override;
      void react(const Event::InitFail &event) override;
      void react(const Event::InitSuccess &event) override;
    };

    struct InitFail : public Fsm
    {
      const std::string name = "init fail";
      void entry(void) override;
      void react(const Event::Restart &event) override;
    };

    struct InitSuccess : public Fsm
    {
      const std::string name = "init success";
      void entry(void) override;
      void react(const Event::Open &event) override;
    };

    struct Open : public Fsm
    {
      const std::string name = "open";
      void entry(void) override;
      void react(const Event::OpenFail &event) override;
      void react(const Event::OpenSuccess &event) override;
    };

    struct OpenFail : public Fsm
    {
      const std::string name = "open fail";
      void entry(void) override;
      void react(const Event::Restart &event) override;
    };

    struct OpenSuccess : public Fsm
    {
      const std::string name = "open success";
      void entry(void) override;
      void react(const Event::Idle &event) override;
    };

    struct Idle : public Fsm
    {
      const std::string name = "idle";
      void entry(void) override;
      void react(const Event::Link &event) override;
      void react(const Event::Read &event) override;
      void react(const Event::Write &event) override;
    };

    struct Link : public Fsm
    {
      const std::string name = "link";
      void entry(void) override;
      void react(const Event::LinkFail &event) override;
      void react(const Event::LinkSuccess &event) override;
    };

    struct LinkFail : public Fsm
    {
      const std::string name = "link fail";
      void entry(void) override;
      void react(const Event::Restart &event) override;
    };

    struct LinkSuccess : public Fsm
    {
      const std::string name = "link success";
      void entry(void) override;
      void react(const Event::Read &event) override;
      void react(const Event::Write &event) override;
    };

    struct Read : public Fsm
    {
      const std::string name = "read";
      void entry(void) override;
      void react(const Event::ReadFail &event) override;
      void react(const Event::ReadSuccess &event) override;
    };

    struct ReadFail : public Fsm
    {
      const std::string name = "read fail";
      void entry(void) override;
      void react(const Event::Restart &event) override;
    };

    struct ReadSuccess : public Fsm
    {
      const std::string name = "read success";
      void entry(void) override;
      void react(const Event::Idle &event) override;
    };

    struct Write : public Fsm
    {
      const std::string name = "write";
      void entry(void) override;
      void react(const Event::WriteFail &event) override;
      void react(const Event::WriteSuccess &event) override;
    };

    struct WriteFail : public Fsm
    {
      const std::string name = "write fail";
      void entry(void) override;
      void react(const Event::Restart &event) override;
    };

    struct WriteSuccess : public Fsm
    {
      const std::string name = "write success";
      void entry(void) override;
      void react(const Event::Idle &event) override;
    };

    struct Restart : public Fsm
    {
      const std::string name = "restart";
      void entry(void) override;
      void react(const Event::Close &event) override;
    };
  };
};

struct Package
{
  struct Event
  {
    struct Begin : tinyfsm::Event {};
    struct Init  : tinyfsm::Event {};

    struct Idle     : tinyfsm::Event {};
    struct ReadDrive : tinyfsm::Event {};

    struct PackageCheck    : tinyfsm::Event {};
    struct PackageExist    : tinyfsm::Event {};
    struct PackageNotExist : tinyfsm::Event {};

    struct Restart : tinyfsm::Event {};
  };

  class Fsm : public tinyfsm::Fsm<Fsm>
  {
    public:

      std::string getNowState(void);

      virtual void entry(void);
      virtual void exit(void);

      virtual void react(const Event::Begin &event);
      virtual void react(const Event::Init &event);
      virtual void react(const Event::Idle &event);
      virtual void react(const Event::ReadDrive &event);
      virtual void react(const Event::PackageCheck &event);
      virtual void react(const Event::PackageExist &event);
      virtual void react(const Event::PackageNotExist &event);
      virtual void react(const Event::Restart &event);

      void react(const tinyfsm::Event&);

    protected:

      void setNowState(std::string state);

    private:

      static std::string now_state_;
  };

  struct State
  {
    struct Begin : public Fsm
    {
      const std::string name = "begin";
      void entry(void) override;
      void react(const Event::Init &event) override;
    };

    struct Init : public Fsm
    {
      const std::string name = "init";
      void entry(void) override;
      void react(const Event::Idle &event) override;
    };

    struct Idle : public Fsm
    {
      const std::string name = "idle";
      void entry(void) override;
      void react(const Event::ReadDrive &event) override;
    };

    struct ReadDrive : public Fsm
    {
      const std::string name = "read drive";
      void entry(void) override;
      void react(const Event::PackageCheck &event) override;
    };

    struct PackageCheck : public Fsm
    {
      const std::string name = "package check";
      void entry(void) override;
      void react(const Event::PackageExist &event) override;
      void react(const Event::PackageNotExist &event) override;
    };

    struct PackageTimeoutCheck : public Fsm
    {
      const std::string name = "package timeout check";
      void entry(void) override;
      void react(const Event::Idle &event) override;
      void react(const Event::Restart &event) override;
    };

    struct Restart : public Fsm
    {
      const std::string name = "restart";
      void entry(void) override;
      void react(const Event::Begin &event) override;
    };
  };
};

}

#endif
