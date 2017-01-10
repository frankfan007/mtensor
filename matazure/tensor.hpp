﻿#pragma once

#include <matazure/algorithm.hpp>

namespace matazure {

template <typename _Tensor>
class tensor_expression {
public:
	typedef _Tensor						tensor_type;

	MATAZURE_GENERAL const tensor_type &operator()() const {
		return *static_cast<const tensor_type *>(this);
	}

	MATAZURE_GENERAL tensor_type &operator()() {
		return *static_cast<tensor_type *>(this);
	}

protected:
	MATAZURE_GENERAL tensor_expression() {}
	MATAZURE_GENERAL ~tensor_expression() {}
private:
	const tensor_expression& operator= (const tensor_expression &);
};

template <typename _Type, int_t ..._SArgs>
class static_tensor : public tensor_expression<static_tensor<_Type, _SArgs...>> {
private:
	template <int_t ..._Ext>
	struct traits;

	template <int_t _S0>
	struct traits<_S0> {
		MATAZURE_GENERAL static constexpr int_t size() {
			return _S0;
		}

		MATAZURE_GENERAL static constexpr pointi<1> stride() {
			return pointi<1>{ { _S0 } };
		}
	};

	template <int_t _S0, int_t _S1>
	struct traits<_S0, _S1> {
		MATAZURE_GENERAL static constexpr int_t size() {
			return _S0 * _S1;
		}

		MATAZURE_GENERAL static constexpr pointi<2> stride() {
			return{ { _S0, _S0 * _S1 } };
		}
	};

	template <int_t _S0, int_t _S1, int_t _S2>
	struct traits<_S0, _S1, _S2> {
		MATAZURE_GENERAL static constexpr int_t size() {
			return _S0 * _S1 * _S2;
		}

		MATAZURE_GENERAL static constexpr pointi<3> stride() {
			return{ { _S0, _S0 * _S1, _S0 * _S1 * _S2 } };
		}
	};

	template <int_t _S0, int_t _S1, int_t _S2, int_t _S3>
	struct traits<_S0, _S1, _S2, _S3> {
		MATAZURE_GENERAL static constexpr int_t size() {
			return _S0 * _S1 * _S2 * _S3;
		}

		MATAZURE_GENERAL static constexpr pointi<4> stride() {
			return{ { _S0, _S0 * _S1, _S0 * _S1 * _S2, _S0 * _S1 * _S2 * _S3 } };
		}
	};

	typedef traits<_SArgs...> traits_t;

public:
	static	const int_t			dim = sizeof...(_SArgs);
	typedef _Type					value_type;
	typedef value_type *			pointer;
	typedef const pointer			const_pointer;
	typedef value_type &			reference;
	typedef const value_type &		const_reference;
	typedef linear_access_t			access_type;
	typedef matazure::pointi<dim>	extent_type;
	typedef pointi<dim>				index_type;
	typedef local_t					memory_type;

	MATAZURE_GENERAL constexpr extent_type stride() const {
		return traits_t::stride();
	}

	MATAZURE_GENERAL constexpr extent_type extent() const {

		return{ _SArgs ... };
	}

	MATAZURE_GENERAL constexpr const_reference operator()(const pointi<dim> &idx) const {
		return (*this)[index2offset(idx, stride(), first_major_t{})];
	}

	MATAZURE_GENERAL reference operator()(const pointi<dim> &idx) {
		return (*this)[index2offset(idx, stride(), first_major_t{})];
	}

	template <typename ..._Idx>
	MATAZURE_GENERAL constexpr const_reference operator()(_Idx... idx) const {
		return (*this)(index_type{ idx... });
	}

	template <typename ..._Idx>
	MATAZURE_GENERAL reference operator()(_Idx... idx) {
		return (*this)(index_type{ idx... });
	}

	MATAZURE_GENERAL constexpr const_reference operator[](int_t i) const { return elements_[i]; }

	MATAZURE_GENERAL reference operator[](int_t i) { return elements_[i]; }

	MATAZURE_GENERAL constexpr int_t size() const { return traits_t::size(); }

	MATAZURE_GENERAL const_pointer data() const {
		return elements_;
	}

	MATAZURE_GENERAL pointer data() {
		return elements_;
	}

public:
	value_type			elements_[traits_t::size()];
};

template <typename _Type, int_t _Dim, typename _Layout = first_major_t>
class tensor : public tensor_expression<tensor<_Type, _Dim, _Layout>> {
public:
	static_assert(std::is_pod<_Type>::value, "only supports pod type now");

	static const int_t						dim = _Dim;
	typedef _Type							value_type;

	typedef matazure::pointi<dim>			extent_type;
	typedef pointi<dim>						index_type;
	typedef _Layout							layout_type;
	typedef linear_access_t					access_type;
	typedef host_t							memory_type;

public:
	tensor() :
		tensor(extent_type::zeros())
	{ }

	template <typename ..._Ext>
	explicit tensor(_Ext... ext) :
		tensor(extent_type{ext...})
	{}

	explicit tensor(extent_type extent) :
		extent_(extent),
		stride_(get_stride(extent)),
		sp_data_(malloc_shared_memory(stride_[dim - 1])),
		data_(sp_data_.get())
	{ }

	explicit tensor(extent_type extent, std::shared_ptr<value_type> sp_data) :
		extent_(extent),
		stride_(get_stride(extent)),
		sp_data_(sp_data),
		data_(sp_data_.get())
	{ }

	template <typename ..._Idx>
	value_type& operator()(_Idx... idx) const {
		return (*this)(index_type{ idx... });
	}

	value_type & operator()(const index_type &index) const {
		return (*this)[index2offset(index, stride_, layout_type{})];
	}

	value_type& operator[](int_t i) const { return data_[i]; }

	extent_type extent() const { return extent_; }
	extent_type stride() const { return stride_; }

	int_t size() const { return stride_[dim - 1]; }

	shared_ptr<value_type> shared_data() const { return sp_data_; }
	value_type * data() const { return sp_data_.get(); }

private:
	shared_ptr<value_type> malloc_shared_memory(int_t size) {
	#ifndef MATAZURE_CUDA
		value_type *data = new decay_t<value_type>[size];
		return shared_ptr<value_type>(data, [](value_type *ptr) {
			delete[] ptr;
		});
	#else
		decay_t<value_type> *data = nullptr;
		cuda::throw_on_error(cudaMallocHost(&data, size * sizeof(value_type)));
		return shared_ptr<value_type>(data, [](value_type *ptr) {
			cuda::throw_on_error(cudaFreeHost(const_cast<decay_t<value_type> *>(ptr)));
		});
	#endif
	}

private:
	const extent_type	extent_;
	const extent_type	stride_;
	const shared_ptr<value_type>	sp_data_;
	value_type * const data_;
};

template <typename _Type>
using matrix = tensor<_Type, 2>;

template <typename _Type, int_t _Col, int_t _Row>
using static_matrix = static_tensor<_Type, _Col, _Row>;

namespace detail {

template <int_t _Dim, typename _Func>
struct get_functor_accessor_type {
private:
	typedef function_traits<_Func>						functor_traits;
	static_assert(functor_traits::arguments_size == 1, "functor must be unary");
	typedef	decay_t<typename functor_traits::template arguments<0>::type> _tmp_type;

public:
	typedef conditional_t<
		is_same<int_t, _tmp_type>::value,
		linear_access_t,
		conditional_t<is_same<_tmp_type, pointi<_Dim>>::value, array_access_t, void>
	> type;
};

}

template <int_t _Dim, typename _Func>
class lambda_tensor : public tensor_expression<lambda_tensor<_Dim, _Func>> {
	typedef function_traits<_Func>						functor_traits;
public:
	static const int_t										dim = _Dim;
	typedef typename functor_traits::result_type			value_type;
	typedef matazure::pointi<dim>							extent_type;
	typedef pointi<dim>										index_type;
	typedef typename detail::get_functor_accessor_type<_Dim, _Func>::type
															access_type;
	typedef host_t											memory_type;

public:
	lambda_tensor(const extent_type &extent, _Func fun) :
		extent_(extent),
		stride_(matazure::get_stride(extent)),
		fun_(fun)
	{}

	value_type operator()(index_type index) const {
		return index_imp<access_type>(index);
	}

	template <typename ..._Idx>
	value_type operator()(_Idx... idx) const {
		return (*this)(index_type{ idx... });
	}

	value_type operator[](int_t i) const {
		return offset_imp<access_type>(i);
	}

	tensor<value_type, dim> persist() const {
		tensor<value_type, dim> re(this->extent());
		copy(*this, re);
		return re;
	}

	template <int_t _S, int_t ..._Extents>
	static_tensor<value_type, _S, _Extents...> persist() const {
		static_tensor<value_type, _S, _Extents...> re;
		copy(*this, re);
		return re;
	}

	extent_type extent() const { return extent_; }
	extent_type stride() const { return stride_; }
	int_t size() const { return stride_[dim - 1]; }

private:
	template <typename _Mode>
	enable_if_t<is_same<_Mode, array_access_t>::value, value_type> index_imp(index_type index) const {
		return fun_(index);
	}

	template <typename _Mode>
	enable_if_t<is_same<_Mode, linear_access_t>::value, value_type> index_imp(index_type index) const {
		return (*this)[index2offset(index, stride(), first_major_t{})];
	}

	template <typename _Mode>
	enable_if_t<is_same<_Mode, array_access_t>::value, value_type> offset_imp(int_t i) const {
		return (*this)(offset2index(i, stride(), first_major_t{}));
	}

	template <typename _Mode>
	enable_if_t<is_same<_Mode, linear_access_t>::value, value_type> offset_imp(int_t i) const {
		return fun_(i);
	}

private:
	const extent_type extent_;
	const extent_type stride_;
	const _Func fun_;
};

}
