#include "circuit.h"
#include <cmath>

namespace r8 {

namespace {

constexpr int MaxM = Netlist::MaxNodes + Netlist::MaxOA + 1;
constexpr int MaxRhs = Netlist::MaxC + 1;

// Gaussian elimination with partial pivoting, several right-hand sides, in place
template <typename T> bool solve(T (&M)[MaxM][MaxM], T (&X)[MaxM][MaxRhs], int n, int nrhs) {
  for (int i = 0; i < n; i++) {
    int p = i;
    double best = std::abs(M[i][i]);
    for (int r = i + 1; r < n; r++) {
      double a = std::abs(M[r][i]);
      if (a > best) { best = a; p = r; }
    }
    if (!(best > 1e-300)) return false;
    if (p != i) {
      for (int c = 0; c < n; c++) { T t = M[i][c]; M[i][c] = M[p][c]; M[p][c] = t; }
      for (int c = 0; c < nrhs; c++) { T t = X[i][c]; X[i][c] = X[p][c]; X[p][c] = t; }
    }
    T inv = T(1) / M[i][i];
    for (int r = i + 1; r < n; r++) {
      T f = M[r][i] * inv;
      if (f == T(0)) continue;
      for (int c = i; c < n; c++) M[r][c] -= f * M[i][c];
      for (int c = 0; c < nrhs; c++) X[r][c] -= f * X[i][c];
    }
  }
  for (int i = n - 1; i >= 0; i--) {
    T inv = T(1) / M[i][i];
    for (int c = 0; c < nrhs; c++) {
      T v = X[i][c];
      for (int k = i + 1; k < n; k++) v -= M[i][k] * X[k][c];
      X[i][c] = v * inv;
    }
  }
  return true;
}

template <typename T> void stamp(T (&M)[MaxM][MaxM], int a, int b, T y) {
  if (a >= 0) M[a][a] += y;
  if (b >= 0) M[b][b] += y;
  if (a >= 0 && b >= 0) { M[a][b] -= y; M[b][a] -= y; }
}

// conductances, op-amps and the input source; capacitors are added by the caller
template <typename T> int build(const Netlist& n, T (&M)[MaxM][MaxM]) {
  const int N = n.nodes, size = N + n.noa + 1;
  for (int i = 0; i < size; i++)
    for (int j = 0; j < size; j++) M[i][j] = T(0);
  for (int i = 0; i < n.nr; i++) stamp(M, n.R[i].a, n.R[i].b, T(1.0 / n.R[i].r));
  for (int k = 0; k < n.noa; k++) {
    const auto& o = n.OA[k];
    const int col = N + k;
    M[o.o][col] += T(1);                 // the output current enters the output node
    if (o.p >= 0) M[col][o.p] += T(1);   // constraint: v+ - v- = 0
    if (o.m >= 0) M[col][o.m] -= T(1);
  }
  const int src = N + n.noa;
  M[n.in][src] += T(1);
  M[src][n.in] += T(1);
  return size;
}

} // namespace

std::complex<double> response(const Netlist& n, double f) {
  using cd = std::complex<double>;
  static thread_local cd M[MaxM][MaxM];
  static thread_local cd X[MaxM][MaxRhs];
  const int size = build(n, M);
  const cd s(0.0, 2.0 * 3.14159265358979323846 * f);
  for (int i = 0; i < n.nc; i++) stamp(M, n.C[i].a, n.C[i].b, s * n.C[i].c);
  for (int i = 0; i < size; i++) X[i][0] = 0;
  X[size - 1][0] = 1.0;
  if (!solve(M, X, size, 1)) return 0.0;
  return X[n.out][0];
}

bool discretize(const Netlist& n, double fs, int K, double* A, double* B, double* C, double& D) {
  if (n.nc != K) return false;
  double M[MaxM][MaxM];
  double X[MaxM][MaxRhs];
  const int size = build(n, M);
  double gc[Netlist::MaxC];
  for (int i = 0; i < K; i++) {
    gc[i] = 2.0 * n.C[i].c * fs;               // trapezoidal companion conductance
    stamp(M, n.C[i].a, n.C[i].b, gc[i]);
  }
  // right-hand sides: one per capacitor history source, one for the input voltage
  for (int i = 0; i < size; i++)
    for (int c = 0; c <= K; c++) X[i][c] = 0;
  for (int k = 0; k < K; k++) {
    if (n.C[k].a >= 0) X[n.C[k].a][k] += 1;
    if (n.C[k].b >= 0) X[n.C[k].b][k] -= 1;
  }
  X[size - 1][K] = 1;
  if (!solve(M, X, size, K + 1)) return false;
  auto v = [&](int node, int col) { return node >= 0 ? X[node][col] : 0.0; };
  for (int i = 0; i < K; i++) {
    const int a = n.C[i].a, b = n.C[i].b;
    for (int j = 0; j < K; j++) A[i * K + j] = 2 * gc[i] * (v(a, j) - v(b, j)) - (i == j ? 1.0 : 0.0);
    B[i] = 2 * gc[i] * (v(a, K) - v(b, K));
  }
  for (int j = 0; j < K; j++) C[j] = v(n.out, j);
  D = v(n.out, K);
  for (int i = 0; i < K * K; i++) if (!std::isfinite(A[i])) return false;
  return true;
}

} // namespace r8
