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

#include <map>
#include <set>
#include "api/Worker.h"
#include "full_text/FullText.h"

BOOST_AUTO_TEST_SUITE(link_counter)

BOOST_AUTO_TEST_CASE(link_counter) {

	std::map<size_t, std::map<size_t, float>> counter;

	HashTableHelper::truncate("test_main_index");
	FullText::count_link_batch("test_main_index", "ALEXANDRIA-TEST-01", counter);

	BOOST_CHECK_EQUAL(counter.size(), 9);
}

BOOST_AUTO_TEST_CASE(link_counter2) {

	std::map<size_t, std::set<size_t>> counter;

	Config::link_batches = {"ALEXANDRIA-TEST-01", "ALEXANDRIA-TEST-02"};

	HashTableHelper::truncate("test_main_index");

	Worker::Status status;
	FullText::count_all_links("test_main_index", status);

	File::TsvFileRemote top_links("urls/link_counts/top_1.gz");

	vector<string> top_urls;
	top_links.read_column_into(0, top_urls);

	BOOST_REQUIRE_EQUAL(top_urls.size(), 13);
	BOOST_CHECK(top_urls[0] == "http://url7.com/test");
}

BOOST_AUTO_TEST_SUITE_END()