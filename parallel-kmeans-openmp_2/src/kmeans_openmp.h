#ifndef KMEANS_OPENMP_H
#define KMEANS_OPENMP_H

#include "kmeans_serial.h"

KMeansResult kmeans_openmp(const Dataset &data, const KMeansConfig &config);

#endif
