/*
Prometheus.h
by Raj Sharma
rsharma@uiuc.edu

Standard declarations which are included in all projects.
*/

#ifndef PROMETHEUS_H
#define PROMETHEUS_H


typedef const char * PS_RESULT;

#define DEFINE_ERROR(x, y)  PS_RESULT x=y
#define DECLARE_ERROR(x)  extern PS_RESULT x

DECLARE_ERROR(PS_OK);
DECLARE_ERROR(PS_FAIL);


#endif
