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

#pragma once

#include "common/log/log.h"
#include "sql/operator/aggregate_physical_operator.h"
#include "storage/record/record.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
#include "sql/stmt/delete_stmt.h"

/*
void int_to_String(int intDate,std::string &strDate){
  int year=0;
  int month=0;
  int day=0;
  int res=0;
  year= intDate/10000 ; 
  month=(intDate-year*10000)/100;
  day=intDate%100;
    
    std::stringstream ss;
    ss << year;
    ss<<'-';
    if(month<10){
      ss<<res;
    }
    ss << month;
    ss<<'-';
    if(day<10){
      ss<<res;
    }
    ss << day;
    strDate=ss.str();
}
*/

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
  RC rc = RC::SUCCESS;
  if (result_tuple_.cell_num()>0) {
    return RC::RECORD_EOF;
  }

  PhysicalOperator *oper = children_[0].get();
  std::vector<Value> result_cells(aggregations_.size(),Value());
  int curnum=0; //统计插图元组个数的
  std::vector<int> res={0,0};
  while (RC::SUCCESS == (rc = oper->next())) {
    curnum++;
    Tuple *tuple = oper->current_tuple();
    for(int cell_idx =0;cell_idx<(int)aggregations_.size();cell_idx++){
      const AggrOp aggregation=aggregations_[cell_idx];
      Value cell;
      AttrType attr_type = AttrType::INTS;
      switch (aggregation){
        case AggrOp::AGGR_SUM:
          res[0]=1;
          rc=tuple->cell_at(cell_idx,cell);
          attr_type =cell.attr_type();
          if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
            result_cells[cell_idx].set_float(result_cells[cell_idx].get_float()+cell.get_float());
          }break;


        case AggrOp::AGGR_MAX:
          res[0]=1;
          rc=tuple->cell_at(cell_idx,cell);
          attr_type =cell.attr_type();
          if(result_cells[cell_idx].attr_type()==AttrType::UNDEFINED){
            if(attr_type==AttrType::CHARS){
              result_cells[cell_idx].set_string(cell.get_string().c_str());
              result_cells[cell_idx].set_type(AttrType::CHARS);
            }
            else if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
              result_cells[cell_idx].set_type(AttrType::FLOATS);
              result_cells[cell_idx].set_float(cell.get_float());
            }
            else if(attr_type==AttrType::DATES){
              result_cells[cell_idx].set_type(AttrType::DATES);
              result_cells[cell_idx].set_date(cell.get_date());
            }
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::INTS or result_cells[cell_idx].attr_type() == AttrType::FLOATS){
            if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
            result_cells[cell_idx].set_float(max(result_cells[cell_idx].get_float(),cell.get_float()));
            }
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::CHARS){
            result_cells[cell_idx].set_string(result_cells[cell_idx].get_string() >cell.get_string()? result_cells[cell_idx].get_string() .c_str():cell.get_string().c_str());
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::DATES){
            result_cells[cell_idx].set_date(max(result_cells[cell_idx].get_date(),cell.get_date()));
          }break;


          case AggrOp::AGGR_MIN:
          res[0]=1;
          rc=tuple->cell_at(cell_idx,cell);
          attr_type =cell.attr_type();
          if(result_cells[cell_idx].attr_type()==AttrType::UNDEFINED){
            if(attr_type==AttrType::CHARS){
              result_cells[cell_idx].set_string(cell.get_string().c_str());
              result_cells[cell_idx].set_type(AttrType::CHARS);
            }
            else if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
              result_cells[cell_idx].set_type(AttrType::FLOATS);
              result_cells[cell_idx].set_float(cell.get_float());
            }
            else if(attr_type==AttrType::DATES){
              result_cells[cell_idx].set_type(AttrType::DATES);
              result_cells[cell_idx].set_date(cell.get_date());
            }
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::INTS or result_cells[cell_idx].attr_type() == AttrType::FLOATS){
            if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
            result_cells[cell_idx].set_float(min(result_cells[cell_idx].get_float(),cell.get_float()));
            }
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::CHARS){
            result_cells[cell_idx].set_string(result_cells[cell_idx].get_string() <cell.get_string()? result_cells[cell_idx].get_string() .c_str():cell.get_string().c_str());
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::DATES){
            result_cells[cell_idx].set_date(min(result_cells[cell_idx].get_date(),cell.get_date()));
          }break;


          case AggrOp::AGGR_AVG:
          res[0]=1;
          rc=tuple->cell_at(cell_idx,cell);
          attr_type =cell.attr_type();
          if(result_cells[cell_idx].attr_type()==AttrType::UNDEFINED){
            if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
              result_cells[cell_idx].set_type(AttrType::FLOATS);
              result_cells[cell_idx].set_float(cell.get_float());
            }
          }
          else if(result_cells[cell_idx].attr_type()==AttrType::INTS or result_cells[cell_idx].attr_type() == AttrType::FLOATS){
            if(attr_type ==AttrType::INTS or attr_type == AttrType::FLOATS){
            result_cells[cell_idx].set_float((result_cells[cell_idx].get_float()*(curnum-1)+cell.get_float())/curnum);
            }
          }break;

          case AggrOp::AGGR_COUNT:
          res[0]=1;
          rc=tuple->cell_at(cell_idx,cell);
          attr_type =cell.attr_type();
          if(result_cells[cell_idx].attr_type()==AttrType::UNDEFINED){
              result_cells[cell_idx].set_type(AttrType::INTS);
              result_cells[cell_idx].set_int(1);

          }
          else{
          result_cells[cell_idx].set_int(result_cells[cell_idx].get_int()+1);
          }break;

          case AggrOp::AGGR_COUNT_ALL:
          res[0]=1;
          rc=tuple->cell_at(cell_idx,cell);
          attr_type =cell.attr_type();
          if(result_cells[cell_idx].attr_type()==AttrType::UNDEFINED){
              result_cells[cell_idx].set_type(AttrType::INTS);
              result_cells[cell_idx].set_int(1);
          }
          //我感觉这已经是石山代码了，全是复用，code copy 但是不想改了-------------------------------------------------
          else{
          result_cells[cell_idx].set_int(result_cells[cell_idx].get_int()+1);
          }break;

        default:
          res[1]=1;

      }
    }
  }
  if(rc== RC::RECORD_EOF){
    rc= RC::SUCCESS;
  }
  if(res[0]==1&&res[1]==1){
    return RC::RECORD_EOF;
  }
  else{


  result_tuple_.set_cells(result_cells);
  return rc;
  }
}

RC AggregatePhysicalOperator::close()
{
  if (!children_.empty()) {
    children_[0]->close();
  }
  return RC::SUCCESS;
}

Tuple *AggregatePhysicalOperator::current_tuple(){
      return &result_tuple_;
}
