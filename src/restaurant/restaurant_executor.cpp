/*********************************************************************
*  Software License Agreement (BSD License)
*
*   Copyright (c) 2018, Intelligent Robotics
*   All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without
*   modification, are permitted provided that the following conditions
*   are met:

*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of Intelligent Robotics nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.

*   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*   POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Jonatan Gines jonatan.gines@urjc.es */

/* Mantainer: Jonatan Gines jonatan.gines@urjc.es */

#include "./restaurant_executor.h"
#include <string>
#include <list>
#include <vector>

RestaurantExecutor::RestaurantExecutor():
  nh_(),
  current_goal_(),
  tf_listener_(tfBuffer_),
  utils_(nh_)
{
  init_knowledge();
  order_ready_asked_ = false;
  order_delivered_ = false;
  new_customer_ = false;
}

void RestaurantExecutor::init_knowledge()
{
  robot_id_ = "sonny";

  add_instance("robot", robot_id_);
  add_instance("person", "barman");
  add_instance("person", "new_customer");
  add_instance("zone", "waiting_zone");

  add_predicate("person_at new_customer wp_waiting_zone");
  add_predicate("person_at barman wp_bar");
  add_predicate("wp_in_zone wp_waiting_zone waiting_zone");

  add_predicate("robot_at " + robot_id_ + " wp_entry");
  add_predicate("robot_at_room " + robot_id_ + " main_room");

  add_predicate("person_at_room new_customer main_room");
  add_predicate("person_at new_customer wp_waiting_zone");
  add_predicate("wp_in_zone wp_waiting_zone waiting_zone");

  add_predicate("wp_bar_location wp_bar");
  add_predicate("wp_entry_location wp_entry");
  add_predicate("barman barman");

  add_instance("table", "table_0");
  add_instance("table", "table_1");
  add_instance("table", "table_2");
  add_instance("table", "table_3");
  add_instance("table", "table_4");
  add_instance("table", "table_5");
  add_instance("table", "bar");

  add_predicate("is_wp_near_table wp_table_0 table_0");
  add_predicate("is_wp_near_table wp_table_1 table_1");
  add_predicate("is_wp_near_table wp_table_2 table_2");
  add_predicate("is_wp_near_table wp_table_3 table_3");
  add_predicate("is_wp_near_table wp_table_4 table_4");
  add_predicate("is_wp_near_table wp_table_5 table_5");
  add_predicate("is_wp_near_table wp_bar bar");

  add_predicate("is_table_at table_0 main_room");
  add_predicate("is_table_at table_1 main_room");
  add_predicate("is_table_at table_2 main_room");
  add_predicate("is_table_at table_3 main_room");
  add_predicate("is_table_at table_4 main_room");
  add_predicate("is_table_at table_5 main_room");
  add_predicate("is_table_at bar main_room");

  graph_.begin_batch();
  graph_.add_node(robot_id_, "robot");
  graph_.add_node("barman", "person");
  graph_.add_node("new_customer", "person");

  graph_.add_node("main_room", "room");
  graph_.set_tf_identity("main_room", "map");
  graph_.set_tf_identity("base_footprint", robot_id_);
  graph_.add_tf_edge("main_room", robot_id_);

  int num_tables_to_check;
  nh_.param<int>("/restaurant_executor_node/num_tables_to_check", num_tables_to_check, 6);
  tf2::Quaternion q;
  q.setRPY(0, 0, 0);

  for (int i = 0; i < num_tables_to_check; i++)
  {
    std::string table = "table_" + std::to_string(i);
    graph_.add_node(table, "table");
    graph_.add_node("wp_"+ table, "waypoint");  // node is redundantelly added by graph-kms sync issue
    graph_.add_edge(table, "needs_check", table);
    tf2::Transform wp2table(q, tf2::Vector3(0.9, 0.0, 0.0));
    graph_.add_edge("wp_" + table, wp2table, table, true);
  }

  graph_.add_node("wp_bar", "waypoint");
  graph_.add_node("bar", "table");
  graph_.add_node("waiting_zone", "zone");

  tf2::Transform wp2table(q, tf2::Vector3(0.9, 0.0, 0.0));
  graph_.add_edge("wp_bar", wp2table, "bar", true);
  tf2::Transform main2zone(q, tf2::Vector3(-0.8, -1.0, 0.0));
  graph_.add_edge("main_room", main2zone, "waiting_zone", true);
  graph_.flush();
  utils_.set_inital_pose(0.0, 0.0, 0.0);
}

bool RestaurantExecutor::update()
{
  return ok();
}

std::vector<std::string> RestaurantExecutor::splitSpaces(std::string raw_str)
{
  std::vector<std::string> output;
  std::istringstream iss(raw_str);
  std::string s;
  while (getline(iss, s, ' '))
  {
    output.push_back(s);
  }
  return output;
}

void RestaurantExecutor::setNewGoal(std::string goal)
{
  remove_current_goal();
  current_goal_ = goal;
  add_goal(current_goal_);
  call_planner();
}

void RestaurantExecutor::Init_code_once()
{
  graph_.add_edge(robot_id_, "ask: bar_start.action", robot_id_);
}

void RestaurantExecutor::Init_code_iterative()
{
}

void RestaurantExecutor::checkTableStatus_code_once()
{
  graph_.add_edge(robot_id_, "robot_status: Recognizing the shop status", robot_id_);
  graph_.add_edge(robot_id_, "say: I will check the table status.", robot_id_);
}

void RestaurantExecutor::checkTableStatus_code_iterative()
{
  std::vector<bica_graph::StringEdge> tables_to_check = graph_.get_string_edges_by_data("needs_check");
  if (!tables_to_check.empty())
    setNewGoal("table_checked " + tables_to_check[0].get_source());
}

void RestaurantExecutor::idle_code_iterative()
{
  if (order_delivered_)
    setNewGoal("robot_at " + robot_id_ + " wp_waiting");
}

void RestaurantExecutor::getOrder_code_once()
{
  graph_.add_edge(robot_id_, "robot_status: Getting new order", robot_id_);
  graph_.add_edge(robot_id_, "say: I will take the order.", robot_id_);
}

void RestaurantExecutor::getOrder_code_iterative()
{
  setNewGoal(current_goal_);
}

void RestaurantExecutor::setOrder_code_iterative()
{
  setNewGoal("order_to_barman " + robot_id_);
}

void RestaurantExecutor::checkOrder_code_iterative()
{
  setNewGoal("order_checked " + robot_id_);
}

void RestaurantExecutor::fixOrder_code_iterative()
{
  remove_predicate("order_needs_fix " + robot_id_);
  setNewGoal("order_fixed " + robot_id_);
}

void RestaurantExecutor::deliverOrder_code_iterative()
{
  graph_.add_edge(robot_id_, "robot_status: Delivering order", robot_id_);
  setNewGoal("order_delivered " + needs_serving_table_);
}

void RestaurantExecutor::grettingNewCustomer_code_once()
{
  graph_.add_edge(robot_id_, "robot_status: Greeting new costumer", robot_id_);
  // graph_.add_edge(robot_id_,
  //  "say: Hi! Welcome to the restaurant, I will guide you to a table. Follow me", robot_id_);
}

void RestaurantExecutor::grettingNewCustomer_code_iterative()
{
  setNewGoal("attended_person new_customer " + ready_table_);
}

bool RestaurantExecutor::Init_2_checkTableStatus()
{
  std::vector<bica_graph::StringEdge> response_edges =
    graph_.get_string_edges_by_data("response: [[:alnum:]_]*");

  bool exists_response = false;
  for (auto edge : response_edges)
  {
    graph_.remove_edge(edge.get_source(), edge.get(), edge.get_target());
    exists_response = true;
  }
  return exists_response;
}

bool RestaurantExecutor::checkTableStatus_2_idle()
{
  if (graph_.get_string_edges_by_data("needs_check").empty())
    setNewGoal("robot_at " + robot_id_ + " wp_waiting");

  return graph_.get_string_edges_by_data("robot_at")[0].get_target() == "wp_waiting";
}

bool RestaurantExecutor::idle_2_getOrder()
{
  std::vector<bica_graph::StringEdge> interest_edges =
    graph_.get_string_edges_by_data("status: needs_serving");

  if (!interest_edges.empty() && !order_delivered_)
  {
    ROS_ERROR("--------- idle_2_getOrder transiting 1");
    needs_serving_table_ = interest_edges[0].get_source();
    ROS_ERROR("--------- idle_2_getOrder transiting 2");

    current_goal_ = "order_ready " + interest_edges[0].get_source();
    ROS_ERROR("--------- idle_2_getOrder transiting 3");

    return true;
  }
  else
    return false;
}

bool RestaurantExecutor::getOrder_2_setOrder()
{
  return (!(search_predicates_regex(current_goal_)).empty());
}

bool RestaurantExecutor::setOrder_2_checkOrder()
{
  bool ret = false;

  if (!(search_predicates_regex(current_goal_)).empty())
  {
    auto interest_edges = graph_.get_string_edges_from_node_by_data("barman", "response: [[:alnum:]_]*");
    if (!interest_edges.empty())
    {
      graph_.remove_edge(interest_edges[0]);
      order_ready_asked_ = false;
      ret = true;
    }
    else if (!order_ready_asked_)
    {
      order_ready_asked_ = true;
      std::string str = "Please, tell me Its ready when the order is in the table";
      graph_.add_edge(robot_id_, "say: " + str, "barman");
      graph_.add_edge(robot_id_, "ask: orderReady.ask", "barman");
    }
  }

  return ret;
}

bool RestaurantExecutor::checkOrder_2_fixOrder()
{
  return (!(search_predicates_regex("order_needs_fix " + robot_id_)).empty());
}

bool RestaurantExecutor::checkOrder_2_deliverOrder()
{
  return (!(search_predicates_regex(current_goal_)).empty());
}

bool RestaurantExecutor::fixOrder_2_checkOrder()
{
  bool ret = false;

  if (!(search_predicates_regex(current_goal_)).empty())
  {
    auto interest_edges = graph_.get_string_edges_from_node_by_data("barman", "response: [[:alnum:]_]*");

    if (!interest_edges.empty())
    {
      graph_.remove_edge(interest_edges[0]);
      order_ready_asked_ = false;
      remove_predicate("order_checked " + robot_id_);
      ret = true;
    }
    else if (!order_ready_asked_)
    {
      order_ready_asked_ = true;
      std::string str = "Please, tell me Its ready when the order is in the table";
      graph_.add_edge(robot_id_, "say: " + str, "barman");
      graph_.add_edge(robot_id_, "ask: orderReady.ask", "barman");
    }
  }

  return ret;
}

bool RestaurantExecutor::deliverOrder_2_idle()
{
  bool ret = false;

  if (!(search_predicates_regex(current_goal_)).empty())
  {
    auto interest_edges = graph_.get_string_edges_from_node_by_data(robot_id_, "response: [[:alnum:]_]*");

    if (!interest_edges.empty())
    {
      graph_.remove_edge(interest_edges[0]);
      order_ready_asked_ = false;
      order_delivered_ = true;
      ret = true;
    }
    else if (!order_ready_asked_)
    {
      order_ready_asked_ = true;
      graph_.add_edge(robot_id_, "ask: confirmOrder.ask", needs_serving_table_);
    }
  }

  return ret;
}

bool RestaurantExecutor::idle_2_grettingNewCustomer()
{
  std::vector<bica_graph::StringEdge> edges_list = graph_.get_string_edges_by_data("status: ready");
  if (!edges_list.empty() && order_delivered_ && !new_customer_ /*&& person_close()*/)
  {
    new_customer_ = true;
    ready_table_ = edges_list[0].get_source();
    return true;
  }
  else
    return false;
}

bool RestaurantExecutor::grettingNewCustomer_2_idle()
{
  return (!(search_predicates_regex(current_goal_)).empty());
}
