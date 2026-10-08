#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "rcs/types.hpp"
#include <vector>

class Calibration {
public:
    static int argMax(const std::vector<double> &v);
    static void meanPowerProfile(const ComplexMatrix &data, std::vector<double> &profile);
    static void cropAroundPeak(ComplexMatrix &data, std::vector<double> &profile,
                               std::vector<double> &R, int peak, int cropN);

    static void gateWindowFromPeak(const std::vector<double> &prof, int gp, double critDb, double fac,
                                   int N, double riseTolDb, int &g1, int &g2, int &bw);
    static void gateNetPower(const std::vector<double> &prof, int g1, int g2, int N, double &Pnet,
                             double &bgPer);

    static int snapPeak1D(const std::vector<double> &prof, int gp, int win, int N);

    static double cornerEnergy2D(const ComplexMatrix &focused, int pkR, int g1c, int g2c);

    static void toRcsProfile(const std::vector<double> &profile, const std::vector<double> &R,
                             double sigmaCalLin, double Pcorner, double Rcorner,
                             std::vector<double> &rcsDbsm);
};

#endif
