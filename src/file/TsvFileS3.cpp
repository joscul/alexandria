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

#include "TsvFileS3.h"
#include "system/Logger.h"

TsvFileS3::TsvFileS3(const Aws::S3::S3Client &s3_client, const string &file_name) {
	// Check if the file exists.

	m_s3_client = s3_client;
	m_file_name = file_name;

	ifstream infile(get_path());

	if (!infile.good()) {
		download_file();
	}

	set_file_name(get_path());
}

TsvFileS3::TsvFileS3(const Aws::S3::S3Client &s3_client, const string &bucket, const string &file_name) {
	// Check if the file exists.

	m_s3_client = s3_client;
	m_bucket = bucket;
	m_file_name = file_name;

	ifstream infile(get_path());

	if (!infile.good()) {
		download_file();
	}

	set_file_name(get_path());
}

TsvFileS3::~TsvFileS3() {
	
}

string TsvFileS3::get_path() const {
	return string(TSV_FILE_DESTINATION) + "/" + m_file_name;
}

int TsvFileS3::download_file() {
	Aws::S3::Model::GetObjectRequest request;

	const string bucket = get_bucket();

	if (m_file_name.find(".gz") == m_file_name.size() - 3) {
		m_is_gzipped = true;
	} else {
		m_is_gzipped = false;
	}

	LOG_INFO("Downloading file from " + bucket + " key: " + m_file_name);
	request.SetBucket(bucket);
	request.SetKey(m_file_name);

	auto outcome = m_s3_client.GetObject(request);

	if (outcome.IsSuccess()) {

		create_directory();
		ofstream outfile(get_path(), ios::trunc);

		if (outfile.good()) {
			auto &stream = outcome.GetResultWithOwnership().GetBody();

			if (m_is_gzipped) {
				filtering_istream in;
				in.push(gzip_decompressor());
				in.push(stream);
				outfile << in.rdbuf();
			} else {
				outfile << stream.rdbuf();
			}
		} else {
			return CC_ERROR;
		}

	} else {
		return CC_ERROR;
	}

	LOG_INFO("Done downloading file from " + bucket + " key: " + m_file_name);

	return CC_OK;
}

string TsvFileS3::get_bucket() {
	if (m_bucket.size()) return m_bucket;
	if (System::is_dev()) {
		return TSV_FILE_BUCKET;
	}
	return TSV_FILE_BUCKET_DEV;
}

void TsvFileS3::create_directory() {
	boost::filesystem::path path(get_path());
	boost::filesystem::create_directories(path.parent_path());
}
