/*
 * MIT License
 *
 * Alexandria.org
 *
 * Copyright (c) 2021 Josef Cullhed, <info@alexandria.org>, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

namespace indexer {
	/*
	This is the base class for the record stored on disk. Needs to be small!
	*/
	#pragma pack(4)
	class generic_record {

		public:
		uint64_t m_value;
		float m_score;
		uint32_t m_count = 1;

		generic_record() : m_value(0), m_score(0.0f) {};
		generic_record(uint64_t value) : m_value(value), m_score(0.0f) {};
		generic_record(uint64_t value, float score) : m_value(value), m_score(score) {};

		size_t count() const { return (size_t)m_count; }

		bool operator==(const generic_record &b) const {
			return m_value == b.m_value;
		}

		bool operator<(const generic_record &b) const {
			return m_value < b.m_value;
		}

		struct storage_order {
			inline bool operator() (const generic_record &a, const generic_record &b) {
				return a.m_value < b.m_value;
			}
		};

		generic_record operator+(const generic_record &b) const {
			generic_record sum;
			sum.m_value = m_value;
			sum.m_count = m_count + b.m_count;
			return sum;
		}

		generic_record &operator+=(const generic_record &b) {
			m_count += b.m_count;
			return *this;
		}

	};
}