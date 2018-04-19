#ifndef CNN_HPP
#define CNN_HPP

#include <armadillo>
#include <vector>
#include <iostream>
#include <cmath>
#include <cassert>

#define DEBUG true
#define DEBUG_PREFIX "[DEBUG ]\t"

class ConvolutionLayer
{
 public:
  ConvolutionLayer(
      size_t inputHeight,
      size_t inputWidth,
      size_t inputDepth,
      size_t filterHeight,
      size_t filterWidth,
      size_t horizontalStride,
      size_t verticalStride,
      size_t numFilters) :
    inputHeight(inputHeight),
    inputWidth(inputWidth),
    inputDepth(inputDepth),
    filterHeight(filterHeight),
    filterWidth(filterWidth),
    horizontalStride(horizontalStride),
    verticalStride(verticalStride),
    numFilters(numFilters)
  {
    // Initialize the filters.
    filters.resize(numFilters);
    for (size_t i=0; i<numFilters; i++)
    {
      filters[i] = arma::zeros(filterHeight, filterWidth, inputDepth);
      filters[i].imbue( [&]() { return _getTruncNormalVal(0.0, 1.0); } );
    }
    _resetAccumulatedGradients();
    #if DEBUG
    for (size_t i=0; i<numFilters; i++)
    {
      std::cout << DEBUG_PREFIX << "Filter #" << i << std::endl;
      std::cout << DEBUG_PREFIX << arma::size(filters[i]) << std::endl;
      for (size_t sidx=0; sidx<inputDepth; sidx++)
      {
        std::cout << DEBUG_PREFIX << "  Slice # " << sidx << std::endl;
        for (size_t ridx=0; ridx<filterHeight; ridx++)
          std::cout << DEBUG_PREFIX << filters[i].slice(sidx).row(ridx);
      }
    }
    #endif
  }

  void Forward(arma::cube& input, arma::cube& output)
  {
    assert((inputHeight - filterHeight)%verticalStride == 0);
    assert((inputWidth - filterWidth)%horizontalStride == 0);
    output = arma::zeros((inputHeight - filterHeight)/verticalStride + 1,
                         (inputWidth - filterWidth)/horizontalStride + 1,
                         numFilters);
    for (size_t fidx = 0; fidx < numFilters; fidx++)
    {
      for (size_t i=0; i <= inputHeight - filterHeight; i += verticalStride)
        for (size_t j=0; j <= inputWidth - filterWidth; j += horizontalStride)
          output(i, j, fidx) = arma::dot(
              arma::vectorise(
                  input.subcube(i, j, 0,
                                i+filterHeight-1, j+filterWidth-1, inputDepth-1)
                ),
              arma::vectorise(filters[fidx]));
    }

    this->input = input;
    this->output = output;
  }

  void Backward(arma::cube& upstreamGradient)
  {
    assert(upstreamGradient.n_slices == numFilters);
    assert(upstreamGradient.n_rows == output.n_rows);
    assert(upstreamGradient.n_cols == output.n_cols);

    gradInput = arma::zeros(arma::size(input));

    for (size_t sidx=0; sidx < numFilters; sidx++)
    {
      for (size_t r=0; r<output.n_rows; r += verticalStride)
      {
        for (size_t c=0; c<output.n_cols; c += horizontalStride)
        {
          arma::cube tmp(arma::size(input), arma::fill::zeros);
          tmp.subcube(r, c, 0, r+filterHeight-1, c+filterWidth-1, inputDepth-1)
              = filters[sidx];
          gradInput += upstreamGradient.slice(sidx)(r,c) * tmp;
        }
      }
    }

    accumulatedGradInput += gradInput;

    gradFilters.clear();
    gradFilters.resize(numFilters);
    for (size_t i=0; i<numFilters; i++)
      gradFilters[i] = arma::zeros(filterHeight, filterWidth, inputWidth);

    for (size_t fidx=0; fidx<numFilters; fidx++)
    {
      for (size_t r=0; r<output.n_rows; r += verticalStride)
      {
        for (size_t c=0; c<output.n_cols; c += horizontalStride)
        {
          arma::cube tmp(arma::size(filters[fidx]), arma::fill::zeros);
          tmp = input.subcube(r, c, 0,
              r+filterHeight-1, c+filterWidth-1, inputDepth-1);
          gradFilters[fidx] += upstreamGradient.slice(fidx)(r,c) * tmp;
        }
      }
    }

    for (size_t fidx=0; fidx<numFilters; fidx++)
      accumulatedGradFilters[fidx] += gradFilters[fidx];
  }

  void UpdateFilterWeights(size_t batchSize)
  {
    for (size_t fidx=0; fidx<numFilters; fidx++)
      filters[fidx] -= (accumulatedGradFilters[fidx]/batchSize);

    _resetAccumulatedGradients();
  }

 private:
  size_t inputHeight;
  size_t inputWidth;
  size_t inputDepth;
  size_t filterHeight;
  size_t filterWidth;
  size_t horizontalStride;
  size_t verticalStride;
  size_t numFilters;

  std::vector<arma::cube> filters;

  double _getTruncNormalVal(double mean, double variance)
  {
    double stddev = sqrt(variance);
    arma::mat candidate = {3.0 * stddev};
    while (std::abs(candidate[0] - mean) > 2.0 * stddev)
      candidate.randn(1, 1);
    return candidate[0];
  }

  void _resetAccumulatedGradients()
  {
    accumulatedGradFilters.clear();
    accumulatedGradFilters.resize(numFilters);
    for (size_t fidx=0; fidx<numFilters; fidx++)
      accumulatedGradFilters[fidx] = arma::zeros(filterHeight,
                                                 filterWidth,
                                                 inputDepth);
    accumulatedGradInput = arma::zeros(inputHeight, inputWidth, inputDepth);
  }

  bool _checkGradients()
  {
    // TODO
  }

  arma::cube input;
  arma::cube output;
  arma::cube gradInput;
  arma::cube accumulatedGradInput;
  std::vector<arma::cube> gradFilters;
  std::vector<arma::cube> accumulatedGradFilters;
};

class PoolingLayer
{
};

#endif
