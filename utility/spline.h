#pragma once
#include	<cmath>
#include	<vector>
#include	"../system/commontypes.h"

#define MaxSplineSize 100

class Spline {
	int num;
	double a[MaxSplineSize + 1];
	double b[MaxSplineSize + 1];
	double c[MaxSplineSize + 1];
	double d[MaxSplineSize + 1];
public:
	Spline() { num = 0; }
	void init(double *sp, int num);
	double culc(double t);
};

void drawSpline(std::vector<Vector3>& points);
std::vector<Vector3> GetSplinePoints(std::vector<Vector3>& points);