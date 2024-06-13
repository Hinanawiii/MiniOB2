//
// Created by AbsoluDe on 2024/4/21.
//

#include "common/log/log.h"
#include "sql/operator/aggregate_physical_operator.h"
#include "storage/record/record.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

RC AggregatePhysicalOperator::open(Trx *trx)
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

  return RC::SUCCESS;
}

RC AggregatePhysicalOperator::next()
{
  if (result_tuple_.cell_num() > 0) {
    return RC::RECORD_EOF;
  }

  RC rc = RC::SUCCESS;
  PhysicalOperator *oper = children_[0].get();

  std::vector<Value> result_cells;

  // for calculating avg, maintain iteration times
  int aggr_size = (int)aggregations_.size();
  int iter_time[aggr_size]; 
  for (int i = 0; i < aggr_size; i++) {
    iter_time[i] = 0;
    //const AggrOp aggregation = aggregations_[i];
    result_cells.push_back(Value());
  }

  
  while (RC::SUCCESS == (rc = oper->next())) {
    // get tuple
    Tuple *tuple = oper->current_tuple();

    // do aggregate
    for (int cell_idx = 0; cell_idx < aggr_size; cell_idx++) {
      const AggrOp aggregation = aggregations_[cell_idx];

      Value cell;
      AttrType attr_type;

      rc = tuple->cell_at(cell_idx, cell);
      attr_type = cell.attr_type();

      if (iter_time[cell_idx] == 0 && aggregation != AggrOp::AGGR_COUNT && aggregation != AggrOp::AGGR_COUNT_ALL) {
        result_cells[cell_idx].set_value(cell);
        iter_time[cell_idx] ++;
        continue;
      }

      switch (aggregation) {
        case AggrOp::AGGR_SUM:
          if (attr_type == AttrType::INTS || attr_type == AttrType::FLOATS) {
            result_cells[cell_idx].set_float(result_cells[cell_idx].get_float() + cell.get_float());
          }
          break;
        case AggrOp::AGGR_MAX:
          if (attr_type == AttrType::INTS || attr_type == AttrType::FLOATS) {
            result_cells[cell_idx].set_float(std::max(result_cells[cell_idx].get_float(), cell.get_float()));
          } else if (attr_type == AttrType::DATES) {
            result_cells[cell_idx].set_date(std::max(result_cells[cell_idx].get_int(), cell.get_int()));
          } else if (attr_type == AttrType::CHARS) {
            if (cell.get_string() > result_cells[cell_idx].get_string()){
              result_cells[cell_idx].set_string(cell.get_string());
            }
          }
          break;
        case AggrOp::AGGR_MIN:
          if (attr_type == AttrType::INTS || attr_type == AttrType::FLOATS) {
            result_cells[cell_idx].set_float(std::min(result_cells[cell_idx].get_float(), cell.get_float()));
          } else if (attr_type == AttrType::DATES) {
            result_cells[cell_idx].set_date(std::min(result_cells[cell_idx].get_int(), cell.get_int()));
          } else if (attr_type == AttrType::CHARS) {
            if (cell.get_string() < result_cells[cell_idx].get_string()){
              result_cells[cell_idx].set_string(cell.get_string());
            }
          }
          break;
        case AggrOp::AGGR_AVG:
          if (attr_type == AttrType::INTS || attr_type == AttrType::FLOATS) {
            float cells_sum = result_cells[cell_idx].get_float() * iter_time[cell_idx] + cell.get_float();
            iter_time[cell_idx] ++;
            result_cells[cell_idx].set_float(cells_sum / iter_time[cell_idx]);
          }
          break;
        case AggrOp::AGGR_COUNT_ALL:
        case AggrOp::AGGR_COUNT:
          result_cells[cell_idx].set_int(result_cells[cell_idx].get_int() + 1);
          break;
        case AggrOp::AGGR_NONE:
          break;
        default:
          rc = RC::UNIMPLENMENT;
          break;
      }
    }
  }

  if (rc == RC::RECORD_EOF) {
    rc = RC::SUCCESS;
  }

  result_tuple_.set_cells(result_cells);

  return rc;
}

RC AggregatePhysicalOperator::close()
{
  if (!children_.empty()) {
    children_[0]->close();
  }
  return RC::SUCCESS;
}

void AggregatePhysicalOperator::add_aggregation(const AggrOp aggregation){
  aggregations_.push_back(aggregation);
}
