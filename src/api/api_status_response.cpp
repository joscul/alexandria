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

#include "api_status_response.h"
#include <system/Profiler.h>

#include "worker/worker.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::ordered_json;

namespace api {

	api_status_response::api_status_response(worker::status &status) {

		json message;

		message["status"] = "indexing";
		message["progress"] = (double)status.items_indexed / status.items;
		message["items"] = status.items;
		message["items_indexed"] = status.items_indexed;

		double time_left = 0.0;
		if (status.items_indexed > 0) {
			const size_t items_left = status.items - status.items_indexed;
			const double time_per_item = ((double)(Profiler::timestamp() - status.start_time)) / (double)status.items_indexed;
			time_left = (double)items_left * time_per_item;
		}
		message["time_left"] = time_left;

		//m_response = message.dump();
		m_response = message.dump(4);
	}

	api_status_response::~api_status_response() {

	}

	ostream &operator<<(ostream &os, const api_status_response &api_response) {
		os << api_response.m_response;
		return os;
	}

}
