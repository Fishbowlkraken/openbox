// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; -*-
#ifndef __application_hh
#define __application_hh

#include "eventdispatcher.hh"

namespace otk {

class AppWidget;

class Application : public EventDispatcher {

public:

  Application(int argc, char **argv);
  virtual ~Application();

  inline int screen() const { return _screen; }
  
  virtual void run(void);
  // more bummy cool functionality

  void setDockable(bool dockable) { _dockable = dockable; }
  inline bool isDockable(void) const { return _dockable; }

private:
  void loadStyle(void);

  int _screen;
  bool _dockable;

  int _appwidget_count;

  friend class AppWidget;
};

}

#endif
