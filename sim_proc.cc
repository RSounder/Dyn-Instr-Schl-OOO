#include <bits/stdc++.h>

#include "sim_proc.h"

int main(int argc, char *argv[])
{

  params.rob_size = strtoul(argv[1], NULL, 10);
  params.iq_size = strtoul(argv[2], NULL, 10);
  params.width = strtoul(argv[3], NULL, 10);
  trace_file = argv[4];

  renamePipelineReg = decodePipelineReg = fetchPipelineReg =
      vector<vector<int>>(params.width, vector<int>(11));

  regreadPipelineReg = vector<vector<int>>(params.width, vector<int>(11));
  dispatchPipelineReg = vector<vector<int>>(params.width, vector<int>(11));
  writebackPipelineReg = vector<vector<int>>(params.width * 5, vector<int>(11));
  issueQueueVector = vector<vector<int>>(params.iq_size, vector<int>(11));
  ROB = vector<vector<int>>(params.rob_size, vector<int>(11));
  RMT = vector<pair<bool, int>>(67, {false, 0});
  executePipelineReg = vector<vector<int>>(params.width * 5, vector<int>(11));
  tracesDone = false;

  FP = fopen(trace_file, "r");
  if (FP == NULL)
  {
    exit(EXIT_FAILURE);
  }

  do
  {
    Retire();
    Writeback();
    Execute();
    Issue();
    Dispatch();
    RegRead();
    Rename();
    Decode();
    Fetch();
  } while (Advance_Cycle());

  print(argv);

  return 0;
}
