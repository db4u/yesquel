/*
  Original code: Copyright (c) 2014 Microsoft Corporation
  Modified code: Copyright (c) 2015-2016 VMware, Inc
  All rights reserved. 

  Written by Joshua B. Leners

  MIT License

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdio.h>
#include <assert.h>
#include <float.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <sstream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bench-runner.h"
#include "bench-log.h"
#include "bench-mysql-client.h"

void run_client(const char* conf_str, const Config* cfg, bool init_db){
  ClientPtr clp = nullptr;
  int ret;
  // SYSTEM-SPECIFIC
  FlushLog();
  if ((ret = CreateMysqlClient(conf_str, &clp, init_db)) != 0){
  } else {
    Workload workload = getWorkloadFromString(cfg->get<std::string>("workload", ""));
    RunWorkload(clp, workload, cfg);
  }
  delete clp;
  return;
}

void do_experiment(int nthreads, const char* cfg_str, const Config* cfg, bool load){
  std::vector<std::thread> threads;
  for (auto i = 0; i < nthreads; ++i){
    threads.push_back(std::thread(std::bind(run_client, cfg_str, cfg, load && i == 0)));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  for (auto& i : threads){
    i.join();
  }
}

extern int handleOpts(int argc, char *argv[]);
extern int optind;
extern int optClientno;
extern char *usage;
extern char *optServerPort;

int main(int argc, char* argv[])
{  
  argc -= handleOpts(argc, argv);
  if (argc < 2){
    fprintf(stderr, usage, argv[0]);
    exit(1);
  }
  optClientno = atoi(argv[optind+1]);

  if (mysql_library_init(0,0,0)){
    fprintf(stderr, "Cannot initialize mysql library\n");
    exit(1);
  }
  

  ConfigParser cp(argv[optind]);
  cp.Parse();
  bool load_complete = false;

  for (auto& section : cp._sections){
    if (section.first.find("Workload") != std::string::npos){
      std::string workload = section.first;

      Config cfg(&cp, workload);
      // SYSTEM-SPECIFIC
      std::string system_conf;
      if (!optServerPort) system_conf = cfg.get<std::string>("mysql", "");
      else system_conf = std::string(optServerPort);

      std::string dir = cfg.get<std::string>("logdir", ".");
      if (dir.back() != '/')
        dir.append("/");
      dir.append("client-");
      dir.append(std::to_string(optClientno));
      dir.append("-");
      dir.append(workload);
      dir.append(".txt");
      SetLog(dir.c_str());
      LOG("Starting a MYSQL expt.\n");
      int nthreads = cfg.get<int>("threads", 1);
      bool do_load = false;
      if (!load_complete && optClientno == 0){
        LOG("I think I should be doing the loading\n");
        do_load = cfg.get<bool>("load", true);
      } else if (!load_complete){
        LOG("I think I'm not the leader, and this is the first run, so I'm sleeping for a bit\n");
        FlushLog();
        sleep(15);
      }
      do_experiment(nthreads, system_conf.c_str(), &cfg, do_load);
      load_complete = true;
    }
  }

  mysql_library_end();
  // FIXME: call mysql_thread_end() from each thread


  return 0;
}

