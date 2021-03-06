#include <vector>

#include "caffe/data_layers.hpp"
#include "caffe/layer.hpp"
#include "caffe/util/io.hpp"

namespace caffe {

template <typename Dtype>
void MemoryDataLayer<Dtype>::DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
     const vector<Blob<Dtype>*>& top) {
  batch_size_ = this->layer_param_.memory_data_param().batch_size();
  channels_ = this->layer_param_.memory_data_param().channels();
  height_ = this->layer_param_.memory_data_param().height();
  width_ = this->layer_param_.memory_data_param().width();
  size_ = channels_ * height_ * width_;
  CHECK_GT(batch_size_ * size_, 0) <<
      "batch_size, channels, height, and width must be specified and"
      " positive in memory_data_param";
  top[0]->Reshape(batch_size_, channels_, height_, width_);
  top[1]->Reshape(batch_size_, 1, 1, 1);
  added_data_.Reshape(batch_size_, channels_, height_, width_);
  added_label_.Reshape(batch_size_, 1, 1, 1);
  data_ = NULL;
  labels_ = NULL;
  needs_reshape_ = false;
  added_data_.cpu_data();
  added_label_.cpu_data();
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::AddDatumVector(const vector<Datum>& datum_vector) {
  CHECK(!has_new_data_) <<
      "Can't add Datum when earlier ones haven't been consumed"
      << " by the upper layers";
  size_t num = datum_vector.size();
  CHECK_GT(num, 0) << "There is no datum to add";
  CHECK_LE(num % batch_size_, 0) <<
      "The number of added datum must be multiple of the batch size";
  if (num > batch_size_) {
    added_data_.Reshape(num, channels_, height_, width_);
    added_label_.Reshape(num, 1, 1, 1);
  }
  // Apply data transformations (mirror, scale, crop...)
  this->data_transformer_.Transform(datum_vector, &added_data_);
  // Copy Labels
  Dtype* top_label = added_label_.mutable_cpu_data();
  for (int item_id = 0; item_id < num; ++item_id) {
    top_label[item_id] = datum_vector[item_id].label();
  }
  // num_images == batch_size_
  Dtype* top_data = added_data_.mutable_cpu_data();
  Reset(top_data, top_label, num);
  has_new_data_ = true;
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::AddMatVector(const vector<cv::Mat>& mat_vector,
    const vector<int>& labels) {
  size_t num = mat_vector.size();
  CHECK(!has_new_data_) <<
      "Can't add Mat when earlier ones haven't been consumed"
      << " by the upper layers";
  CHECK_LE(num % batch_size_, 0) <<
      "The number of added datum must be multiple of the batch size";
  CHECK_GT(num, 0) << "There is no mat to add";
  if (num > batch_size_) {
    added_data_.Reshape(num, channels_, height_, width_);
    added_label_.Reshape(num, 1, 1, 1);
  }
  // Apply data transformations (mirror, scale, crop...)
  this->data_transformer_.Transform(mat_vector, &added_data_);
  // Copy Labels
  Dtype* top_label = added_label_.mutable_cpu_data();
  for (int item_id = 0; item_id < num; ++item_id) {
    top_label[item_id] = labels[item_id];
  }
  // num_images == batch_size_
  Dtype* top_data = added_data_.mutable_cpu_data();
  Reset(top_data, top_label, num);
  has_new_data_ = true;
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::Reset(Dtype* data, Dtype* labels, int n) {
  CHECK(data);
  CHECK(labels);
  CHECK_EQ(n % batch_size_, 0) << "n must be a multiple of batch size";
  data_ = data;
  labels_ = labels;
  n_ = n;
  pos_ = 0;
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::ChangeBatchSize(int new_size) {
  CHECK(!has_new_data_) <<
      "Can't change batch_size before all data haven't been consumed"
      << " by the upper layers";
  batch_size_ = new_size;
  added_data_.Reshape(batch_size_, channels_, height_, width_);
  added_label_.Reshape(batch_size_, 1, 1, 1);
  needs_reshape_ = true;
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  CHECK(data_) << "MemoryDataLayer needs to be initalized by calling Reset";
  if (needs_reshape_) {
    top[0]->Reshape(batch_size_, channels_, height_, width_);
    top[1]->Reshape(batch_size_, 1, 1, 1);
    needs_reshape_ = false;
  }
  top[0]->set_cpu_data(data_ + pos_ * size_);
  top[1]->set_cpu_data(labels_ + pos_);
  pos_ = (pos_ + batch_size_) % n_;
  if (pos_ == 0)
    has_new_data_ = false;
}

INSTANTIATE_CLASS(MemoryDataLayer);
REGISTER_LAYER_CLASS(MEMORY_DATA, MemoryDataLayer);
}  // namespace caffe
