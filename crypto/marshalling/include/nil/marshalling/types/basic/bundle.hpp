//---------------------------------------------------------------------------//
// Copyright (c) 2017-2021 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2020-2021 Nikita Kaskov <nbering@nil.foundation>
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------//

#ifndef MARSHALLING_BASIC_BUNDLE_HPP
#define MARSHALLING_BASIC_BUNDLE_HPP

#include <type_traits>
#include <algorithm>

#include <nil/marshalling/assert_type.hpp>
#include <nil/marshalling/status_type.hpp>
#include <nil/marshalling/processing/tuple.hpp>

#include <nil/marshalling/types/basic/common_funcs.hpp>

namespace nil {
    namespace marshalling {
        namespace types {
            namespace basic {

                template<typename TFieldBase, typename TMembers>
                class bundle : public TFieldBase {
                public:
                    using value_type = TMembers;
                    using version_type = typename TFieldBase::version_type;

                    bundle() = default;

                    explicit bundle(const value_type &val) : members_(val) {
                    }

                    explicit bundle(value_type &&val) : members_(std::move(val)) {
                    }

                    bundle(const bundle &) = default;

                    bundle(bundle &&) = default;

                    ~bundle() noexcept = default;

                    bundle &operator=(const bundle &) = default;

                    bundle &operator=(bundle &&) = default;

                    const value_type &value() const {
                        return members_;
                    }

                    value_type &value() {
                        return members_;
                    }

                    constexpr std::size_t length() const {
                        return nil::marshalling::processing::tuple_accumulate(value(), std::size_t(0),
                                                                              length_calc_helper());
                    }

                    template<std::size_t TFromIdx>
                    constexpr std::size_t length_from() const {
                        return nil::marshalling::processing::tuple_accumulate_from_until<
                            TFromIdx, std::tuple_size<value_type>::value>(value(), std::size_t(0),
                                                                          length_calc_helper());
                    }

                    template<std::size_t TUntilIdx>
                    constexpr std::size_t length_until() const {
                        return nil::marshalling::processing::tuple_accumulate_from_until<0, TUntilIdx>(
                            value(), std::size_t(0), length_calc_helper());
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx>
                    constexpr std::size_t length_from_until() const {
                        return nil::marshalling::processing::tuple_accumulate_from_until<TFromIdx, TUntilIdx>(
                            value(), std::size_t(0), length_calc_helper());
                    }

                    static constexpr std::size_t min_length() {
                        return nil::marshalling::processing::tuple_type_accumulate<value_type>(
                            std::size_t(0), min_length_calc_helper());
                    }

                    template<std::size_t TFromIdx>
                    static constexpr std::size_t min_length_from() {
                        return nil::marshalling::processing::tuple_type_accumulate_from_until<
                            TFromIdx, std::tuple_size<value_type>::value, value_type>(std::size_t(0),
                                                                                      min_length_calc_helper());
                    }

                    template<std::size_t TUntilIdx>
                    static constexpr std::size_t min_length_until() {
                        return nil::marshalling::processing::tuple_type_accumulate_from_until<0, TUntilIdx, value_type>(
                            std::size_t(0), min_length_calc_helper());
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx>
                    static constexpr std::size_t min_length_from_until() {
                        return nil::marshalling::processing::tuple_type_accumulate_from_until<TFromIdx, TUntilIdx,
                                                                                              value_type>(
                            std::size_t(0), min_length_calc_helper());
                    }

                    static constexpr std::size_t max_length() {
                        return nil::marshalling::processing::tuple_type_accumulate<value_type>(
                            std::size_t(0), max_length_calc_helper());
                    }

                    template<std::size_t TFromIdx>
                    static constexpr std::size_t max_length_from() {
                        return nil::marshalling::processing::tuple_type_accumulate_from_until<
                            TFromIdx, std::tuple_size<value_type>::value, value_type>(std::size_t(0),
                                                                                      max_length_calc_helper());
                    }

                    template<std::size_t TUntilIdx>
                    static constexpr std::size_t max_length_until() {
                        return nil::marshalling::processing::tuple_type_accumulate_from_until<0, TUntilIdx, value_type>(
                            std::size_t(0), max_length_calc_helper());
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx>
                    static constexpr std::size_t max_length_from_until() {
                        return nil::marshalling::processing::tuple_type_accumulate_from_until<TFromIdx, TUntilIdx,
                                                                                              value_type>(
                            std::size_t(0), max_length_calc_helper());
                    }

                    constexpr bool valid() const {
                        return nil::marshalling::processing::tuple_accumulate(value(), true, valid_check_helper());
                    }

                    bool refresh() {
                        return nil::marshalling::processing::tuple_accumulate(value(), false, refresh_helper());
                    }

                    template<typename TIter>
                    status_type read(TIter &iter, std::size_t len) {
                        auto es = status_type::success;
                        nil::marshalling::processing::tuple_for_each(value(), make_read_helper(es, iter, len));
                        return es;
                    }

                    template<std::size_t TFromIdx, typename TIter>
                    status_type read_from(TIter &iter, std::size_t len) {
                        auto es = status_type::success;
                        nil::marshalling::processing::template tuple_for_each_from<TFromIdx>(
                            value(), make_read_helper(es, iter, len));
                        return es;
                    }

                    template<std::size_t TUntilIdx, typename TIter>
                    status_type read_until(TIter &iter, std::size_t len) {
                        auto es = status_type::success;
                        nil::marshalling::processing::template tuple_for_each_until<TUntilIdx>(
                            value(), make_read_helper(es, iter, len));
                        return es;
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx, typename TIter>
                    status_type read_from_until(TIter &iter, std::size_t len) {
                        auto es = status_type::success;
                        nil::marshalling::processing::template tuple_for_each_from_until<TFromIdx, TUntilIdx>(
                            value(), make_read_helper(es, iter, len));
                        return es;
                    }

                    template<typename TIter>
                    void read_no_status(TIter &iter) {
                        nil::marshalling::processing::tuple_for_each(value(), make_read_no_status_helper(iter));
                    }

                    template<std::size_t TFromIdx, typename TIter>
                    void read_from_no_status(TIter &iter) {
                        nil::marshalling::processing::template tuple_for_each_from<TFromIdx>(
                            value(), make_read_no_status_helper(iter));
                    }

                    template<std::size_t TUntilIdx, typename TIter>
                    void read_until_no_status(TIter &iter) {
                        nil::marshalling::processing::template tuple_for_each_until<TUntilIdx>(
                            value(), make_read_no_status_helper(iter));
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx, typename TIter>
                    void read_from_until_no_status(TIter &iter) {
                        nil::marshalling::processing::template tuple_for_each_from_until<TFromIdx, TUntilIdx>(
                            value(), make_read_no_status_helper(iter));
                    }

                    template<typename TIter>
                    status_type write(TIter &iter, std::size_t len) const {
                        auto es = status_type::success;
                        nil::marshalling::processing::tuple_for_each(value(), make_write_helper(es, iter, len));
                        return es;
                    }

                    template<std::size_t TFromIdx, typename TIter>
                    status_type write_from(TIter &iter, std::size_t len) const {
                        auto es = status_type::success;
                        nil::marshalling::processing::template tuple_for_each_from<TFromIdx>(
                            value(), make_write_helper(es, iter, len));
                        return es;
                    }

                    template<std::size_t TUntilIdx, typename TIter>
                    status_type write_until(TIter &iter, std::size_t len) const {
                        auto es = status_type::success;
                        nil::marshalling::processing::template tuple_for_each_until<TUntilIdx>(
                            value(), make_write_helper(es, iter, len));
                        return es;
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx, typename TIter>
                    status_type write_from_until(TIter &iter, std::size_t len) const {
                        auto es = status_type::success;
                        nil::marshalling::processing::template tuple_for_each_from_until<TFromIdx, TUntilIdx>(
                            value(), make_write_helper(es, iter, len));
                        return es;
                    }

                    template<typename TIter>
                    void write_no_status(TIter &iter) const {
                        nil::marshalling::processing::tuple_for_each(value(), make_write_no_status_helper(iter));
                    }

                    template<std::size_t TFromIdx, typename TIter>
                    void write_from_no_status(TIter &iter) const {
                        nil::marshalling::processing::template tuple_for_each_from<TFromIdx>(
                            value(), make_write_no_status_helper(iter));
                    }

                    template<std::size_t TUntilIdx, typename TIter>
                    void write_until_no_status(TIter &iter) const {
                        nil::marshalling::processing::template tuple_for_each_until<TUntilIdx>(
                            value(), make_write_no_status_helper(iter));
                    }

                    template<std::size_t TFromIdx, std::size_t TUntilIdx, typename TIter>
                    void write_from_until_no_status(TIter &iter) const {
                        nil::marshalling::processing::template tuple_for_each_from_until<TFromIdx, TUntilIdx>(
                            value(), make_write_no_status_helper(iter));
                    }

                    static constexpr bool is_version_dependent() {
                        return common_funcs::are_members_version_dependent<value_type>();
                    }

                    bool set_version(version_type version) {
                        return common_funcs::set_version_for_members(value(), version);
                    }

                private:
                    struct length_calc_helper {
                        template<typename TField>
                        constexpr std::size_t operator()(std::size_t sum, const TField &field) const {
                            return sum + field.length();
                        }
                    };

                    struct min_length_calc_helper {
                        template<typename TField>
                        constexpr std::size_t operator()(std::size_t sum) const {
                            return sum + TField::min_length();
                        }
                    };

                    struct max_length_calc_helper {
                        template<typename TField>
                        constexpr std::size_t operator()(std::size_t sum) const {
                            return sum + TField::max_length();
                        }
                    };

                    struct valid_check_helper {
                        template<typename TField>
                        constexpr bool operator()(bool soFar, const TField &field) const {
                            return soFar && field.valid();
                        }
                    };

                    struct refresh_helper {
                        template<typename TField>
                        bool operator()(bool soFar, TField &field) const {
                            return field.refresh() || soFar;
                        }
                    };

                    template<typename TIter>
                    class read_helper {
                    public:
                        read_helper(status_type &es, TIter &iter, std::size_t len) : es_(es), iter_(iter), len_(len) {
                        }

                        template<typename TField>
                        void operator()(TField &field) {
                            if (es_ != nil::marshalling::status_type::success) {
                                return;
                            }

                            es_ = field.read(iter_, len_);
                            if (es_ == nil::marshalling::status_type::success) {
                                len_ -= field.length();
                            }
                        }

                    private:
                        status_type &es_;
                        TIter &iter_;
                        std::size_t len_;
                    };

                    template<typename TIter>
                    static read_helper<TIter> make_read_helper(nil::marshalling::status_type &es, TIter &iter,
                                                               std::size_t len) {
                        return read_helper<TIter>(es, iter, len);
                    }

                    template<typename TIter>
                    class read_no_status_helper {
                    public:
                        read_no_status_helper(TIter &iter) : iter_(iter) {
                        }

                        template<typename TField>
                        void operator()(TField &field) {
                            field.read_no_status(iter_);
                        }

                    private:
                        TIter &iter_;
                    };

                    template<typename TIter>
                    static read_no_status_helper<TIter> make_read_no_status_helper(TIter &iter) {
                        return read_no_status_helper<TIter>(iter);
                    }

                    template<typename TIter>
                    class write_helper {
                    public:
                        write_helper(status_type &es, TIter &iter, std::size_t len) : es_(es), iter_(iter), len_(len) {
                        }

                        template<typename TField>
                        void operator()(const TField &field) {
                            if (es_ != nil::marshalling::status_type::success) {
                                return;
                            }

                            es_ = field.write(iter_, len_);
                            if (es_ == nil::marshalling::status_type::success) {
                                len_ -= field.length();
                            }
                        }

                    private:
                        status_type &es_;
                        TIter &iter_;
                        std::size_t len_;
                    };

                    template<typename TIter>
                    static write_helper<TIter> make_write_helper(status_type &es, TIter &iter, std::size_t len) {
                        return write_helper<TIter>(es, iter, len);
                    }

                    template<typename TIter>
                    class write_no_status_helper {
                    public:
                        write_no_status_helper(TIter &iter) : iter_(iter) {
                        }

                        template<typename TField>
                        void operator()(const TField &field) {
                            field.write_no_status(iter_);
                        }

                    private:
                        TIter &iter_;
                    };

                    template<typename TIter>
                    static write_no_status_helper<TIter> make_write_no_status_helper(TIter &iter) {
                        return write_no_status_helper<TIter>(iter);
                    }

                    static_assert(nil::marshalling::processing::is_tuple<value_type>::value,
                                  "value_type must be tuple");
                    value_type members_;
                };

            }    // namespace basic
        }        // namespace types
    }            // namespace marshalling
}    // namespace nil
#endif    // MARSHALLING_BASIC_BUNDLE_HPP
