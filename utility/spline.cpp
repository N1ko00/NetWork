#include	"spline.h"

//スプラインデータ初期化
void Spline::init(double *sp, int spnum)
{
	double tmp, w[MaxSplineSize + 1];
	int i;

	num = spnum - 1;

	// ３次多項式の0次係数(a)を設定
	for (i = 0; i <= num; i++) {
		a[i] = sp[i];
	}

	// ３次多項式の2次係数(c)を計算
	// 連立方程式を解く。
	// 但し、一般解法でなくスプライン計算にチューニングした方法
	for (i = 0; i <= num; i++) {
		c[i] = 0.0;
	}
//	c[0] = c[num] = 0.0;


	for (i = 1; i < num; i++) {
		c[i] = 3.0 * (a[i - 1] - 2.0 * a[i] + a[i + 1]);
	}

	// 左下を消す
	w[0] = 0.0;
	for (i = 1; i < num; i++) {
		tmp = 4.0 - w[i - 1];
		c[i] = (c[i] - c[i - 1]) / tmp;
		w[i] = 1.0 / tmp;
	}

	// 右上を消す
	for (i = num - 1; i > 0; i--) {
		c[i] = c[i] - c[i + 1] * w[i];
	}

	// ３次多項式の1次係数(b)と3次係数(b)を計算
	for (i = 0; i <= num; i++) {
		b[i] = 0.0;
		d[i] = 0.0;
	}

	for (i = 0; i < num; i++) {
		d[i] = (c[i + 1] - c[i]) / 3.0;
		b[i] = a[i + 1] - a[i] - c[i] - d[i];
	}
}

//媒介変数(0～num-1の実数）に対する値を計算
double Spline::culc(double t)
{
	int j;
	double dt;
	j = (int)floor(t); // 小数点以下切捨て
	if (j < 0) j = 0; else if (j >= num) j = num - 1; // 丸め誤差を考慮

	dt = t - (double)j;
	return a[j] + (b[j] + (c[j] + d[j] * dt) * dt) * dt;
}

void drawSpline(std::vector<Vector3>& points)
{
	Spline xs, ys, zs;
	double t, m;

	std::vector<double> xlist;
	std::vector<double> ylist;
	std::vector<double> zlist;

	for (auto p : points) {
		xlist.emplace_back(static_cast<double>(p.x));
		ylist.emplace_back(static_cast<double>(p.y));
		zlist.emplace_back(static_cast<double>(p.z));
	}

	xs.init(xlist.data(), static_cast<int>(points.size()));
	ys.init(ylist.data(), static_cast<int>(points.size()));
	zs.init(zlist.data(), static_cast<int>(points.size()));

	m = (double)(points.size() - 1);

//	MoveToEx(hdc, xlist[0], ylist[0], nullptr);

	//	moveTo(x[0], y[0], z[0]);

	for (t = 0.0; t <= m; t += 0.01) {
//		LineTo(hdc, xs.culc(t), ys.culc(t));

		//		lineTo(xs.culc(t), ys.culc(t), zs.culc(t));
	}
}

std::vector<Vector3> GetSplinePoints(std::vector<Vector3>& points)
{
	Spline xs, ys, zs;
	double t, m;

	std::vector<Vector3> pointslist{};

	std::vector<double> xlist;
	std::vector<double> ylist;
	std::vector<double> zlist;

	for (auto p : points) {
		xlist.emplace_back(static_cast<double>(p.x));
		ylist.emplace_back(static_cast<double>(p.y));
		zlist.emplace_back(static_cast<double>(p.z));
	}

	xs.init(xlist.data(), static_cast<int>(points.size()));
	ys.init(ylist.data(), static_cast<int>(points.size()));
	zs.init(zlist.data(), static_cast<int>(points.size()));

	m = (double)(points.size() - 1);

	//	MoveToEx(hdc, xlist[0], ylist[0], nullptr);

		//	moveTo(x[0], y[0], z[0]);

	for (t = 0.0; t <= m; t += 0.01) {
		//		LineTo(hdc, xs.culc(t), ys.culc(t));

		//		lineTo(xs.culc(t), ys.culc(t), zs.culc(t));

		Vector3 p;
		p = Vector3(
			static_cast<float>(xs.culc(t)), 
			static_cast<float>(ys.culc(t)),
			static_cast<float>(zs.culc(t)));


		pointslist.emplace_back(p);

	}

	return pointslist;
}

// Ｂスプライン描画
// x[num], y[num], z[num] は座標の配列
void drawSpline(double *x, double *y, double *z, int num)
{
	Spline xs, ys, zs;
	double t, m;
	xs.init(x, num);
	ys.init(y, num);
	zs.init(z, num);
	m = (double)(num - 1);

//	MoveToEx(hdc, x[0], y[0], nullptr);

	//	moveTo(x[0], y[0], z[0]);

	for (t = 0.0; t <= m; t += 0.01) {
//		LineTo(hdc, xs.culc(t), ys.culc(t));

		//		lineTo(xs.culc(t), ys.culc(t), zs.culc(t));
	}
}