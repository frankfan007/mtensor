﻿#pragma once

#include <matazure/cuda/tensor.hpp>

namespace matazure {
namespace cuda {

namespace puzzle {


} //puzzle

} //cuda
} //matazure

#define MATAZURE_PUZZEL_CONV_GLOBAL(conv_global, mask)														\
namespace matazure{namespace cuda{ namespace puzzle {														\
																											\
namespace internal {																						\
																											\
template <typename _Tensor>																					\
struct conv_op {																							\
private:																									\
	_Tensor ts_;																							\
public:																										\
	conv_op(_Tensor ts) :																					\
		ts_(ts)																								\
	{}																										\
																											\
	MATAZURE_GENERAL typename _Tensor::value_type operator()(const pointi<_Tensor::rank> &idx) const {		\
		auto mask_radius = mask.shape() / 2;																\
		auto sum = zero<typename _Tensor::value_type>::value();												\
		device::for_index(pointi<_Tensor::rank>::zeros(), mask.shape(), [&] (const pointi<2> &mask_idx) {	\
			sum += ts_(idx + mask_idx - mask_radius) * mask(mask_idx);										\
		});																									\
		return sum;																							\
	}																										\
};																											\
																											\
}																											\
																											\
template <typename _Tensor>																					\
inline auto conv_global(_Tensor ts)																			\
->decltype(make_lambda(ts.shape(), internal::conv_op<_Tensor>(ts), typename _Tensor::memory_type{})) {		\
	return make_lambda(ts.shape(), internal::conv_op<_Tensor>(ts), typename _Tensor::memory_type{});		\
}																											\
																											\
}}} //matazure/cuda/puzzle

#define MATAZURE_PUZZEL_CONV_BLOCK(conv_block, mask)													\
namespace matazure { namespace cuda{ namespace puzzle{													\
																										\
template <typename _BlockDim, typename _Tensor, typename _TensorRe>										\
inline tensor<typename _Tensor::value_type, _Tensor::rank> conv_block(_Tensor ts, _TensorRe &ts_re) {	\
	MATAZURE_STATIC_ASSERT_DIM_MATCHED(_Tensor, decltype(mask));										\
	MATAZURE_STATIC_ASSERT_VALUE_TYPE_MATCHED(_Tensor, decltype(mask));									\
	typedef typename _Tensor::value_type value_type;													\
																										\
	constexpr auto block_ext = meta::array_to_pointi(_BlockDim{});										\
	auto grid_ext = ts.shape() / block_ext;																\
	MATAZURE_ASSERT(equal(grid_ext * block_ext, ts.shape()));											\
	MATAZURE_ASSERT(equal(ts.shape(), ts_re.shape()));													\
																										\
	auto mask_extent = mask.shape();																	\
	auto mask_radius = mask_extent / 2;																	\
																										\
	block_for_index<_BlockDim>(grid_ext, [=] __device__ (block_index<_BlockDim> block_idx) {			\
		auto shsts_shape = 																				\
			meta::sub_c(meta::add_c(_BlockDim{}, decltype(mask)::meta_shape()), meta::int_t_c<1>{});	\
		__shared__ static_tensor<value_type, decltype(shsts_shape)> shared_ts_block;					\
																										\
		shared_ts_block(block_idx.local) = ts(block_idx.global + pointi<2>{-1, -1});					\
		shared_ts_block(block_idx.local + pointi<2>{2, 0}) = ts(block_idx.global + pointi<2>{1, -1});	\
		shared_ts_block(block_idx.local + pointi<2>{0, 2}) = ts(block_idx.global + pointi<2>{-1, 1});	\
		shared_ts_block(block_idx.local + pointi<2>{2, 2}) = ts(block_idx.global + pointi<2>{1, 1});	\
		device::barrier();																				\
																										\
		auto sum = zero<value_type>::value();															\
		device::for_index(pointi<_Tensor::rank>::zeros(), mask_extent, [&](const pointi<2> &idx) {		\
			sum += shared_ts_block(block_idx.local + idx) * mask(idx);									\
		});																								\
		ts_re(block_idx.global) = sum;																	\
	});																									\
																										\
	return ts_re;																						\
}																										\
																										\
template <typename _BlockDim, typename _Tensor>															\
inline cuda::tensor<typename _Tensor::value_type, _Tensor::rank> conv_block(_Tensor ts) {				\
	cuda::tensor<typename _Tensor::value_type, _Tensor::rank> ts_re(ts.shape());						\
	conv_block<_BlockDim>(ts, ts_re);																	\
	return ts_re;																						\
}																										\
																										\
}}}

/// 卷积需要使用__constant__的mask，但不同的mask必须以静态的方式分别声明，故而定义一个宏，以便在外面使用
#define MATAZURE_PUZZEL_CONV_BLOCK_WITH_CRACK(conv_block_crack, mask)											\
namespace matazure{namespace cuda{namespace puzzle{																\
																												\
template <typename _BlockDim, typename _Tensor, typename _TensorRe>												\
inline tensor<typename _Tensor::value_type, _Tensor::rank> conv_block_crack(_Tensor ts, _TensorRe &ts_re) {		\
	MATAZURE_STATIC_ASSERT_DIM_MATCHED(_Tensor, decltype(mask));												\
	MATAZURE_STATIC_ASSERT_VALUE_TYPE_MATCHED(_Tensor, decltype(mask));											\
	typedef typename _Tensor::value_type value_type;															\
																												\
	constexpr auto block_ext = meta::array_to_pointi(_BlockDim{});												\
	auto grid_ext = ts.shape() / block_ext;																		\
	MATAZURE_ASSERT(equal(grid_ext * block_ext, ts.shape()));													\
	MATAZURE_ASSERT(equal(ts.shape(), ts_re.shape()));															\
																												\
	auto mask_extent = mask.shape();																			\
	auto mask_radius = mask_extent / 2;																			\
																												\
	block_for_index<_BlockDim>(grid_ext, [=] __device__ (block_index<_BlockDim> block_idx) {					\
		__shared__ static_tensor<value_type, _BlockDim> shared_ts_block;										\
		shared_ts_block(block_idx.local) = ts(block_idx.global);												\
		device::barrier();																						\
																												\
		if (inside(block_idx.local, mask_radius, block_idx.block_extent)) {										\
			value_type sum = 0;																					\
																												\
			device::for_index(pointi<_Tensor::rank>::zeros(), mask_extent, [&](const pointi<2> &idx) {			\
				sum += shared_ts_block(block_idx.local + idx - mask_radius) * mask(idx);						\
			});																									\
																												\
			ts_re(block_idx.global) = sum;																		\
		}																										\
	});																											\
																												\
	return ts_re;																								\
}																												\
																												\
template <typename _BlockDim, typename _Tensor>																	\
inline cuda::tensor<typename _Tensor::value_type, _Tensor::rank> conv_block_crack(_Tensor ts) {					\
	cuda::tensor<typename _Tensor::value_type, _Tensor::rank> ts_re(ts.shape());								\
	conv_block_crack<_BlockDim>(ts, ts_re);																		\
	return ts_re;																								\
}																												\
																												\
}}}	 //end MATAZURE_PUZZEL_CONV_BLOCK_WITH_CRACK
