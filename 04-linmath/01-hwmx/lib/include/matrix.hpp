/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy us a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include "algorithm.hpp"
#include "contiguous_matrix.hpp"
#include "utility.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace throttle {
namespace linmath {

template <typename T> class matrix {
  using value_type = T;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;
  using size_type = typename std::size_t;

  contiguous_matrix<T>        m_contiguous_matrix;
  containers::vector<pointer> m_rows_vec;

  void update_rows_vec() {
    m_rows_vec.reserve(rows());
    std::generate_n(std::back_inserter(m_rows_vec), rows(),
                    [ptr = m_contiguous_matrix.data(), n_cols = cols()]() mutable {
                      auto old_ptr = ptr;
                      ptr += n_cols;
                      return old_ptr;
                    });
  }

public:
  matrix(size_type rows, size_type cols, value_type val = value_type{}) : m_contiguous_matrix{rows, cols, val} {
    update_rows_vec();
  }

  template <std::input_iterator it>
  matrix(size_type rows, size_type cols, it start, it finish) : m_contiguous_matrix{rows, cols, start, finish} {
    update_rows_vec();
  }

  matrix(size_type rows, size_type cols, std::initializer_list<value_type> list)
      : m_contiguous_matrix{rows, cols, list} {
    update_rows_vec();
  }

  matrix(contiguous_matrix<T> &&c_matrix) : m_contiguous_matrix(std::move(c_matrix)) { update_rows_vec(); }

  static matrix zero(size_type rows, size_type cols) { return matrix<T>{rows, cols}; }
  static matrix unity(size_type size) { return matrix{std::move(contiguous_matrix<T>::unity(size))}; }

private:
  class proxy_row {
    pointer m_row, m_past_row;

  public:
    proxy_row() = default;
    proxy_row(pointer row, size_type n_cols) : m_row{row}, m_past_row{m_row + n_cols} {}

    using iterator = utility::contiguous_iterator<value_type>;
    using const_iterator = utility::const_contiguous_iterator<value_type>;

    reference      operator[](size_type index) { return m_row[index]; }
    iterator       begin() const { return iterator{m_row}; }
    iterator       end() const { return iterator{m_past_row}; }
    const_iterator cbegin() const { return const_iterator{m_row}; }
    const_iterator cend() const { return const_iterator{m_past_row}; }
  };

  class const_proxy_row {
    const_pointer m_row, m_past_row;

  public:
    const_proxy_row() = default;
    const_proxy_row(const_pointer row, size_type n_cols) : m_row{row}, m_past_row{m_row + n_cols} {}

    using iterator = utility::const_contiguous_iterator<value_type>;
    using const_iterator = iterator;

    const_reference operator[](size_type index) { return m_row[index]; }
    iterator        begin() const { return iterator{m_row}; }
    iterator        end() const { return iterator{m_past_row}; }
    const_iterator  cbegin() const { return const_iterator{m_row}; }
    const_iterator  cend() const { return const_iterator{m_past_row}; }
  };

public:
  proxy_row       operator[](size_type index) { return proxy_row{m_rows_vec[index], cols()}; }
  const_proxy_row operator[](size_type index) const { return const_proxy_row{m_rows_vec[index], cols()}; }

  size_type rows() const { return m_contiguous_matrix.rows(); }
  size_type cols() const { return m_contiguous_matrix.cols(); }
  bool      square() const { return (cols() == rows()); }

  bool equal(const matrix &other) const {
    bool res = (rows() == other.rows()) && (cols() == other.cols());
    for (size_type i = 0; i < m_rows_vec.size(); i++)
      res = res && std::equal((*this)[i].begin(), (*this)[i].end(), other[i].begin());
    return res;
  }

  matrix &transpose() {
    matrix transposed{cols(), rows()};
    for (size_type i = 0; i < rows(); i++) {
      for (size_type j = 0; j < cols(); j++) {
        transposed[j][i] = (*this)[i][j];
      }
    }

    *this = std::move(transposed);
    return *this;
  }

  void swap_rows(size_type idx1, size_type idx2) { std::swap(m_rows_vec[idx1], m_rows_vec[idx2]); }

  template <typename Comp = std::less<value_type>>
  std::pair<size_type, value_type> max_in_col_greater_eq(size_type col, size_type minimum_row, Comp cmp = Comp{}) {
    size_type max_row_idx = minimum_row;
    auto      rows = this->rows();
    for (size_type row = minimum_row; row < rows; row++)
      max_row_idx = (cmp(std::abs((*this)[max_row_idx][col]), std::abs((*this)[row][col])) ? row : max_row_idx);
    return std::pair<size_type, value_type>{max_row_idx, (*this)[max_row_idx][col]};
  }

  template <typename Comp = std::less<value_type>>
  std::pair<size_type, value_type> max_in_col(size_type col, Comp cmp = Comp{}) {
    return max_in_col_greater_eq(col, 0, cmp);
  }

  void gauss_jordan_elimination() {
    matrix &mat = *this;
    auto    rows = mat.rows();
    auto    cols = mat.cols();
    for (size_type i = 0; i < rows; i++) {
      auto [pivot_row, pivot_elem] = max_in_col_greater_eq(i, i);
      swap_rows(i, pivot_row);
      for (size_type to_elim_row = 0; to_elim_row < rows; to_elim_row++) //!
        if (i != to_elim_row) {
          auto coef = mat[to_elim_row][i] / pivot_elem;
          for (size_type col = 0; col < cols; col++) {
            mat[to_elim_row][col] -= coef * mat[i][col];
          }
        }
    }
  }

  value_type determinant() const requires std::is_integral_v<value_type>;

  value_type determinant() const requires std::is_floating_point_v<value_type> {
    if (!square()) throw std::runtime_error("Mismatched matrix size for determinant");
    matrix tmp = *this;
    tmp.gauss_jordan_elimination();
    value_type res = 1;
    auto       rows = tmp.rows();
    for (size_type i = 0; i < rows; i++)
      res *= tmp[i][i];
    return res;
  }

  matrix &operator*=(value_type rhs) {
    m_contiguous_matrix *= rhs;
    return *this;
  }

  matrix &operator/=(value_type rhs) {
    m_contiguous_matrix /= rhs;
    return *this;
  }

  matrix &operator+=(const matrix &other) {
    if (rows() != other.rows() || cols() != other.cols()) throw std::runtime_error("Mismatched matrix sizes");
    for (size_type i = 0; i < rows(); ++i) {
      auto row_first = (*this)[i];
      auto row_second = other[i];
      std::transform(row_first.begin(), row_first.end(), row_second.cbegin(), row_first.begin(),
                     std::plus<value_type>{});
    }
    return *this;
  }

  matrix &operator-=(const matrix &other) {
    if (rows() != other.rows() || cols() != other.cols()) throw std::runtime_error("Mismatched matrix sizes");
    for (size_type i = 0; i < rows(); ++i) {
      auto row_first = (*this)[i];
      auto row_second = other[i];
      std::transform(row_first.begin(), row_first.end(), row_second.cbegin(), row_first.begin(),
                     std::minus<value_type>{});
    }
    return *this;
  }

  matrix &operator*=(const matrix &rhs) {
    if (cols() != rhs.rows()) throw std::runtime_error("Mismatched matrix sizes");

    matrix res{rows(), rhs.cols()}, t_rhs = rhs;
    t_rhs.transpose();

    for (size_type i = 0; i < rows(); i++) {
      for (size_type j = 0; j < t_rhs.rows(); j++) {
        res[i][j] = algorithm::multiply_accumulate((*this)[i].cbegin(), (*this)[i].cend(), t_rhs[j].cbegin(), 0);
      }
    }

    std::swap(*this, res);
    return *this;
  }
};

// clang-format off
template <typename T> matrix<T> operator*(const matrix<T> &lhs, T rhs) { auto res = lhs; res *= rhs; return res; }
template <typename T> matrix<T> operator*(T lhs, const matrix<T> &rhs) { auto res = rhs; res *= lhs; return res; }

template <typename T> matrix<T> operator+(const matrix<T> &lhs, const matrix<T> &rhs) { auto res = lhs; res += rhs; return res; }
template <typename T> matrix<T> operator-(const matrix<T> &lhs, const matrix<T> &rhs) { auto res = lhs; res -= rhs; return res; }

template <typename T> matrix<T> operator*(const matrix<T> &lhs, const matrix<T> &rhs) { auto res = lhs; res *= rhs; return res; }
template <typename T> matrix<T> operator/(const matrix<T> &lhs, T rhs) { auto res = lhs; res /= rhs; return res; }

template <typename T> bool operator==(const matrix<T> &lhs, const matrix<T> &rhs) { return lhs.equal(rhs); }
template <typename T> bool operator!=(const matrix<T> &lhs, const matrix<T> &rhs) { return !(lhs.equal(rhs)); }

template <typename T> matrix<T> transpose(const matrix<T> &mat) { auto res = mat; res.transpose(); return res; }

// clang-format on

} // namespace linmath
} // namespace throttle