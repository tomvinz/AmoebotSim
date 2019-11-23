/* Copyright (C) 2019 Joshua J. Daymude, Robert Gmyr, and Kristian Hinnenthal.
 * The full GNU GPLv3 can be found in the LICENSE file, and the full copyright
 * notice can be found at the top of main/main.cpp. */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <memory>

#include <QObject>
#include <QTimer>

#include "core/system.h"

class Simulator : public QObject {
  Q_OBJECT
 public:
  Simulator();
  virtual ~Simulator();

  void setSystem(std::shared_ptr<System> _system);
  std::shared_ptr<System> getSystem() const;

 signals:
  void systemChanged(std::shared_ptr<System> _system);
  void stepDurationChanged(int ms);
  void saveScreenshot(const QString filePath);

  void started();
  void stopped();

 public slots:
  void start();
  void stop();
  void step();
  void stepForParticleAt(Node node);
  void setStepDuration(int ms);
  void runUntilTermination();
  void saveScreenshotSetup(const QString filePath);

  int numParticles() const;
  int numObjects() const;
  int numMovements() const;
  int numRounds() const;

 protected:
  QTimer stepTimer;

  std::shared_ptr<System> system;
};

#endif // SIMULATOR_H
