#ifndef XLINK_ASSIGN_IONS_H
#define XLINK_ASSIGN_IONS_H

#include "app/CruxApplication.h"
#include "model/Spectrum.h"
#include "LinkedIonSeries.h"

class XLinkAssignIons : public CruxApplication {
 public:
  XLinkAssignIons();
  ~XLinkAssignIons();

  virtual int main(int argc, char** argv);
  virtual std::string getName() const;
  virtual std::string getDescription() const;
  virtual std::vector<std::string> getArgs() const;
  virtual std::vector<std::string> getOptions() const;
  virtual std::vector< std::pair<std::string, std::string> > getOutputs() const;

 private:
  void print_spectrum(Crux::Spectrum* spectrum, LinkedIonSeries& ion_series);
};

#endif

