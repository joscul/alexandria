
#include "CCUrlIndexer.h"

void run_download_thread(const SubSystem *sub_system, const string &warc_path, int shard, int id) {
	const string bucket = "alexandria-cc-output";
	CCUrlIndexer indexer(sub_system);
	indexer.download(bucket, warc_path, shard, id);
}

void run_indexer_thread(const SubSystem *sub_system, const vector<string> &file_names, int shard) {

	CCUrlIndexer indexer(sub_system);
	indexer.index(file_names, shard);
}

void run_sorter_thread(const SubSystem *sub_system, const vector<string> &chunk, int shard) {

	try {
		CCUrlIndexer indexer(sub_system);
		indexer.sorter(chunk, shard);
	} catch (runtime_error &error) {
		cout << error.what() << endl;
	}
}

void upload_results_thread(SubSystem *sub_system, const string &word, int retries) {

	Aws::S3::Model::PutObjectRequest request;
	request.SetBucket("alexandria-index");
	string key = "CC-MAIN-2021-10/index_" + word + ".tsv.gz";
	request.SetKey(key);

	ifstream infile;
	for (int shard = 0; shard < 8; shard++) {
		infile.open("/mnt/" + to_string(shard) + "/output/index_" + word + ".tsv");
		if (infile.is_open()) {
			break;
		}
	}
	if (!infile.is_open()) {
		cout << "ERROR could not find output file for word: " << word << endl;
		return;
	}

	filtering_istream in;
	in.push(gzip_compressor());
	in.push(infile);

	std::shared_ptr<Aws::StringStream> request_body = Aws::MakeShared<Aws::StringStream>("");
	*request_body << in.rdbuf();
	request.SetBody(request_body);

	Aws::S3::Model::PutObjectOutcome outcome = sub_system->s3_client().PutObject(request);
	if (!outcome.IsSuccess()) {
		// Retry.
		if (retries > 0) {
			cout << "Upload failed, retrying for word: " << word << endl;
			upload_results_thread(sub_system, word, retries - 1);
		}
	}
}

void CCUrlIndexer::run_all() {
	run_all(0);
}

void CCUrlIndexer::run_all(size_t limit) {

	Profiler total_profiler("CCUrlIndexer total");

	vector<thread> threads;

	Aws::SDKOptions options;
	options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Error;
	options.loggingOptions.logger_create_fn = get_logger_factory();

	Aws::InitAPI(options);

	Aws::S3::S3Client s3_client = get_s3_client();

	SubSystem *sub_system = new SubSystem(s3_client);

	string warc_paths_url = "crawl-data/CC-MAIN-2021-10/warc.paths.gz";
	TsvFileS3 warc_paths_file(s3_client, "commoncrawl", warc_paths_url);

	vector<string> warc_paths;
	warc_paths_file.read_column_into(0, warc_paths);

	int id = 1;
	const int num_shards = 8;
	vector<string> input_files;
	map<int, vector<string>> shard_files;
	Profiler download_profiler("Download Files");
	for (const string &warc_path : warc_paths) {

		int shard = id % num_shards;

		thread th(run_download_thread, sub_system, warc_path, shard, id);

		threads.push_back(move(th));

		if (threads.size() == CC_NUM_THREADS_DOWNLOADING) {
			// join threads.
			for (thread &_th : threads) {
				_th.join();
			}
			threads.clear();
		}
		
		string output_file = "/mnt/"+to_string(shard)+"/output_"+to_string(id)+".tsv";
		input_files.push_back(output_file);
		shard_files[shard].push_back(output_file);
		//cout << output_file << endl;
		if (limit && id >= limit) break;
		id++;
	}

	// join threads.
	for (thread &_th : threads) {
		_th.join();
	}
	threads.clear();

	download_profiler.stop();

	cout << "Splitting dictionary into chunks" << endl;

	vector<string> words = sub_system->words();
	vector<vector<string>> chunks;

	cout << "words: " << words.size() << endl;

	//vector_chunk(words, ceil((float)words.size() / CC_NUM_THREADS_INDEXING), chunks);

	cout << "num chunks: " << chunks.size() << endl;
	cout << "num input_files: " << input_files.size() << endl; 
	cout << "read chunk offsets" << endl;


	Profiler split_profiler("Split files");

	for (const auto &iter : shard_files) {
		thread th (run_indexer_thread, sub_system, iter.second, iter.first);
		threads.push_back(move(th));
	}

	// join threads.
	int joined = 0;
	for (thread &_th : threads) {
		cout << "threads joined: " << joined << endl;
		joined++;
		_th.join();
	}
	threads.clear();

	split_profiler.stop();
	return;

	/*CCUrlIndexer indexer(sub_system);
	map<size_t, map<string, pair<size_t, size_t>>> chunk_positions;
	indexer.find_chunk_positions(chunks, input_files, chunk_positions);*/

	/*pair<size_t, size_t> interval = chunk_positions[1]["/mnt/1/output_1.tsv"];
	cout << chunks[1][0] << "-" << chunks[1].back() << endl;
	cout << "interval: [" << interval.first << ", " << interval.second << "]" << endl;

	return;*/

	id = 0;
	for (const vector<string> &chunk : chunks) {
		size_t idx = 0;
		cout << "Running chunk: " << id << endl;
		int shard = id % num_shards;
		//thread th (run_indexer_thread, sub_system, chunk, input_files, shard);
		//threads.push_back(move(th));
		id++;
	}

	// join threads.
	joined = 0;
	for (thread &_th : threads) {
		cout << "threads joined: " << joined << endl;
		joined++;
		_th.join();
	}
	threads.clear();

	split_profiler.stop();
	Profiler sort_profiler("Sorting");

	cout << "Sorting" << endl;

	id = 0;
	for (const vector<string> &chunk : chunks) {
		cout << "Running chunk: " << id << endl;
		int shard = id % num_shards;
		thread th (run_sorter_thread, sub_system, chunk, shard);
		threads.push_back(move(th));
		id++;
	}

	// join threads.
	joined = 0;
	for (thread &_th : threads) {
		cout << "threads joined: " << joined << endl;
		joined++;
		_th.join();
	}
	threads.clear();

	sort_profiler.stop();

	// Uploading
	#ifndef CC_TESTING

		Profiler upload_profiler("Upload");

		ThreadPool pool(CC_NUM_THREADS_UPLOADING);
		std::vector<std::future<string>> results;

		int idx = 0;
		size_t word_size = sub_system->words().size();
		for (const string &word : sub_system->words()) {

			results.emplace_back(
				pool.enqueue([sub_system, word, idx, word_size] {
					upload_results_thread(sub_system, word, 3);
					return word + " done " + to_string(idx) + " out of " + to_string(word_size);
				})
			);
			idx++;
		}

		for(auto && result: results) {
			cout << result.get() << endl;
		}

		upload_profiler.stop();
	#else
		cout << "test mode, skipping upload" << endl;
	#endif

	delete sub_system;

	Aws::ShutdownAPI(options);
}

CCUrlIndexer::CCUrlIndexer(const SubSystem *sub_system) :
m_sub_system(sub_system)
{
}

CCUrlIndexer::~CCUrlIndexer() {
}

void CCUrlIndexer::download(const string &bucket, const string &file, int shard, int id) {

	string key = file;
	key.replace(key.find(".warc.gz"), 8, ".gz");

	BasicUrlData url_data(m_sub_system, shard, id);
	url_data.download(bucket, key);
	url_data.build_index();
}

void CCUrlIndexer::sorter(const vector<string> &words, int shard) {

	// Open all my output files.
	map<string, ofstream> out_files;
	for (const string &word : words) {
		string file_name = "/mnt/"+to_string(shard)+"/output/index_" + word + ".tsv";
		ifstream input_file(file_name);

		if (!input_file.is_open()) {
			throw runtime_error("Could not open file: " + file_name + " error: " + strerror(errno));
		}

		string line;
		vector<string> lines;
		vector<size_t> indices;
		vector<double> scores;
		size_t index = 0;
		while (getline(input_file, line)) {
			stringstream ss(line);
			string col;
			getline(ss, col, '\t'); // word
			getline(ss, col, '\t'); // url
			getline(ss, col, '\t'); // score
			double score = stod(col);
			scores.push_back(score);
			indices.push_back(index);
			lines.push_back(line);
			index++;
		}
		input_file.close();

		sort(indices.begin(), indices.end(), [&](const size_t& a, const size_t& b) {
			return (scores[a] > scores[b]);
		});

		ofstream output_file(file_name, ios::trunc);
		const size_t max_num_lines = 50000;
		size_t line_num = 0;
		for (size_t i : indices) {
			output_file << lines[i] << endl;
			line_num++;
			if (line_num >= max_num_lines) break;
		}
	}
}

void CCUrlIndexer::find_chunk_positions(const vector<vector<string>> &chunks, const vector<string> &input_files,
	map<size_t, map<string, pair<size_t, size_t>>> &container) {
	size_t num = 0;
	Profiler find_positions("Find chunk offsets");
	for (const string &file_name : input_files) {
		TsvFile tsv_file(file_name);
		size_t pos_start = 0;
		size_t chunk_index = 0;
		for (const vector<string> words : chunks) {	
			const string last_word = words.back();
			num++;
			size_t pos_end = tsv_file.find_next_position(last_word);
			container[chunk_index][file_name] = make_pair(pos_start, pos_end);
			pos_start = pos_end;
			chunk_index++;
		}
		cout << file_name << endl;
		find_positions.print();
	}

	cout << "Made search " << num << " times" << endl;
}