/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by WangYunlai on 2022/6/27.
//

#include "common/log/log.h"
#include "sql/operator/update_physical_operator.h"
#include "storage/record/record.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
#include "sql/stmt/update_stmt.h"

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  std::unique_ptr<PhysicalOperator> &child = children_[0];
  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  trx_ = trx;

  return RC::SUCCESS;
}

/*
RC UpdatePhysicalOperator::next()
{
  RC rc = RC::SUCCESS;
  if (children_.empty()) {
    return RC::RECORD_EOF;
  }

  PhysicalOperator *child = children_[0].get();

  std::vector<Record> delete_records;
  std::vector<Record> insert_records;
  while (RC::SUCCESS == (rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record &record = row_tuple->record();

    delete_records.emplace_back(record);

    RC rc = RC::SUCCESS;
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to delete record: %s", strrc(rc));
      return rc;
    }else{
      // find the index of updated value, saving in target_index
      const std::vector<FieldMeta> *table_field_metas = table_->table_meta().field_metas();
      const char *target_field_name = field_.field_name();

      int meta_num = table_field_metas->size();
      int target_index = -1;
      for (int i = 0; i < meta_num; i++) {
        FieldMeta fieldmeta = (*table_field_metas)[i];
        const char *field_name = fieldmeta.name();
        if (0 == strcmp(field_name, target_field_name)){
          target_index = i;
          break;
        }
      }

      // rebuild new record, change the value of target_index
      std::vector<Value> values;
      Record new_record;
      int cell_num = row_tuple->cell_num();

      for (int i = 0; i < cell_num; i++) {
        if (i == target_index) {
          values.push_back(value_);
        } else {
          Value tmp_value;
          row_tuple->cell_at(i, tmp_value);
          values.push_back(tmp_value);
        }
      }

      RC rc = table_->make_record(static_cast<int>(values.size()), values.data(), new_record);
      insert_records.emplace_back(new_record);
    }

    if (delete_records.size() != insert_records.size()) {
      return RC::SQL_SYNTAX;
    }

    for (int i = 0; i < delete_records.size(); i++) {
      rc = trx_->delete_record(table_, delete_records[i]);
      rc = trx_->insert_record(table_, insert_records[i]);
    }
  }

  return RC::RECORD_EOF;
}
*/

RC UpdatePhysicalOperator::next()
{
  RC rc = RC::SUCCESS;
  if (children_.empty()) {
    return RC::RECORD_EOF;
  }

  PhysicalOperator *child = children_[0].get();

  while (RC::SUCCESS == (rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record &record = row_tuple->record();

    // get fieldmeta offset and length
    const char* field_name = field_.field_name();
    const FieldMeta *field_meta = table_->table_meta().field(field_name);

    int offset = field_meta->offset();
    int len = field_meta->len();

    rc = trx_->update_record(table_, record, offset, len, value_);

    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to update record: %s", strrc(rc));
      return rc;
    }
  }

  return RC::RECORD_EOF;
}

RC UpdatePhysicalOperator::close()
{
  if (!children_.empty()) {
    children_[0]->close();
  }
  return RC::SUCCESS;
}
