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

#include "full_text_indexer.h"
#include "full_text.h"
#include "logger/logger.h"
#include "text/text.h"
#include <math.h>

using namespace std;

namespace full_text {

	full_text_indexer::full_text_indexer(int id, const string &db_name, const common::sub_system *sub_system, url_to_domain *url_to_domain)
	: m_indexer_id(id), m_db_name(db_name), m_sub_system(sub_system)
	{
		m_url_to_domain = url_to_domain;
		for (size_t shard_id = 0; shard_id < config::ft_num_shards; shard_id++) {
			full_text_shard_builder<struct full_text_record> *shard_builder =
				new full_text_shard_builder<struct full_text_record>(db_name, shard_id);
			m_shards.push_back(shard_builder);
		}
	}

	full_text_indexer::~full_text_indexer() {
		for (full_text_shard_builder<struct full_text_record> *shard : m_shards) {
			delete shard;
		}
	}

	size_t full_text_indexer::add_stream(vector<hash_table::hash_table_shard_builder *> &shard_builders, basic_istream<char> &stream,
		const vector<size_t> &cols, const vector<float> &scores, const string &batch, mutex &write_mutex) {

		string line;
		size_t added_urls = 0;
		const size_t check_for_full_shards_every = 1000;
		while (getline(stream, line)) {
			vector<string> col_values;
			boost::algorithm::split(col_values, line, boost::is_any_of("\t"));

			URL url(col_values[0]);

			float harmonic = url.harmonic(m_sub_system);

			m_url_to_domain->add_url(url.hash(), url.host_hash());

			uint64_t key_hash = url.hash();

			if (config::index_snippets) {
				shard_builders[key_hash % config::ht_num_shards]->add(key_hash, line + "\t" + batch);
			}

			if (config::index_text) {

				const string site_colon = "site:" + url.host() + " site:www." + url.host() + " " + url.host() + " " + url.domain_without_tld();

				size_t score_index = 0;
				map<uint64_t, float> word_map;

				add_data_to_word_map(word_map, site_colon, 20*harmonic);

				for (size_t col_index : cols) {
					add_expanded_data_to_word_map(word_map, col_values[col_index], scores[score_index]*harmonic);
					score_index++;
				}
				for (const auto &iter : word_map) {
					const uint64_t word_hash = iter.first;
					const size_t shard_id = word_hash % config::ft_num_shards;
					m_shards[shard_id]->add(word_hash, full_text_record{.m_value = key_hash, .m_score = iter.second, .m_domain_hash = url.host_hash()});
				}
				word_map.clear();
			}

			added_urls++;

			if (added_urls % check_for_full_shards_every == 0) {
				write_cache(write_mutex);
			}
		}

		write_cache(write_mutex);

		return added_urls;
	}

	size_t full_text_indexer::write_cache(mutex &write_mutex) {

		vector<full_text_shard_builder<struct full_text_record> *> full_shards;

		for (full_text_shard_builder<struct full_text_record> *shard : m_shards) {
			if (shard->full()) {
				full_shards.push_back(shard);
			}
		}
		if (full_shards.size()) {
			write_mutex.lock();

			ThreadPool pool(config::ft_num_threads_appending);
			std::vector<std::future<void>> results;
			for (full_text_shard_builder<struct full_text_record> *shard : full_shards) {
				results.emplace_back(pool.enqueue([shard] {
					shard->append();
				}));
			}
			
			for (auto &fut : results) {
				fut.get();
			}
		
			write_mutex.unlock();
		}

		return full_shards.size();
	}

	void full_text_indexer::flush_cache(vector<mutex> &write_mutexes) {
		size_t idx = 0;
		for (full_text_shard_builder<struct full_text_record> *shard : m_shards) {
			write_mutexes[idx].lock();
			shard->append();
			write_mutexes[idx].unlock();

			idx++;
		}
	}

	void full_text_indexer::read_url_to_domain() {
		m_url_to_domain->read();
	}

	void full_text_indexer::write_url_to_domain() {
		m_url_to_domain->write(m_indexer_id);
	}

	void full_text_indexer::add_expanded_data_to_word_map(map<uint64_t, float> &word_map, const string &text, float score) const {

		vector<string> words = text::get_expanded_full_text_words(text);
		map<uint64_t, uint64_t> uniq;

		if (config::n_grams > 1) {
			text::words_to_ngram_hash(words, config::n_grams, [&word_map, &uniq, score](const uint64_t hash) {
				if (uniq.find(hash) == uniq.end()) {
					word_map[hash] += score;
					uniq[hash] = hash;
				}
			});
		} else {
			for (const string &word : words) {
				const uint64_t word_hash = algorithm::hash(word);
				if (uniq.find(word_hash) == uniq.end()) {
					word_map[word_hash] += score;
					uniq[word_hash] = word_hash;
				}
			}
		}
	}

	void full_text_indexer::add_data_to_word_map(map<uint64_t, float> &word_map, const string &text, float score) const {

		vector<string> words = text::get_full_text_words(text);
		map<uint64_t, uint64_t> uniq;
		for (const string &word : words) {
			const uint64_t word_hash = m_hasher(word);
			if (uniq.find(word_hash) == uniq.end()) {
				word_map[word_hash] += score;
				uniq[word_hash] = word_hash;
			}
		}
	}

	void full_text_indexer::add_data_to_shards(const URL &url, const string &text, float score) {

		vector<string> words = text::get_full_text_words(text);
		for (const string &word : words) {

			const uint64_t word_hash = m_hasher(word);
			const size_t shard_id = word_hash % config::ft_num_shards;

			m_shards[shard_id]->add(word_hash, full_text_record{.m_value = url.hash(), .m_score = score, .m_domain_hash = url.host_hash()});
		}
	}
}
