#include "Types.h"

void registerRcsMetaTypes()
{
    qRegisterMetaType<HrrpResult>("HrrpResult");
    qRegisterMetaType<RcsResultBundle>("RcsResultBundle");
    qRegisterMetaType<UserSelection>("UserSelection");
    qRegisterMetaType<QVector<PickPoint> >("QVector<PickPoint>");
    qRegisterMetaType<QVector<double> >("QVector<double>");
}
