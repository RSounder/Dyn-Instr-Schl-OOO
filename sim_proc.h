#ifndef SIM_PROC_H
#define SIM_PROC_H

using namespace std;

typedef struct proc_params
{
  int rob_size;
  int iq_size;
  int width;
} proc_params;

FILE *FP;                      // File handler
char *trace_file;              // Variable that holds trace file name;
proc_params params;            // look at sim_proc.h header file for the definition of proc_params
int op_type, dest, src1, src2; // Variables are read from trace file
unsigned long int pc;          // Variable holds the pc read from input file

vector<int> wakeupVector;
vector<pair<bool, int>> RMT;
vector<vector<int>> ROB, issueQueueVector, fetchPipelineReg, decodePipelineReg, renamePipelineReg,
    regreadPipelineReg, dispatchPipelineReg, writebackPipelineReg, executePipelineReg, instructionsVector;

int robHeadPtr = 0,
    robTailPtr = 0,
    uniqueVal = 0,
    simCycle = 0;

bool priorFetch, tracesDone;

void invalidateBundle(vector<vector<int>> &bundle,
                      int idx = 0)
{
  for (vector<int> &bdl : bundle)
  {
    bdl[idx] = false;
  }
}

// opt is one of 3 values. 0 means 1 ex cycle, 1 means 2 ex cycles, 2 means 5 ex cycles
int getTimeForOp(int op)
{
  if (op == 0)
    return 1;
  if (op == 1)
    return 2;
  if (op == 2)
    return 5;
  return -1;
}

bool bundleExists(vector<vector<int>> &bundles)
{
  for (vector<int> &bdl : bundles)
  {
    if (bdl[0])
      return true;
  }
  return false;
}

bool bundleCount(vector<int> bdl) { return bdl[0]; }

void Retire()
{
  // Retire up to WIDTH consecutive
  // “ready” instructions from the head of
  // the ROB.

  for (size_t i = 0; i < params.width; i++)
  {
    if (!ROB[robHeadPtr][1])
      return;
    ROB[robHeadPtr][1] = 0;
    instructionsVector[ROB[robHeadPtr][5]].back() = simCycle;

    if (ROB[robHeadPtr][0] >= 1 and
        RMT[ROB[robHeadPtr][0]].second == robHeadPtr)
    {
      RMT[ROB[robHeadPtr][0]].first = false;
    }

    for (size_t j = 0; j < params.width; j++)
    {
      if ((dispatchPipelineReg[j][6] == robHeadPtr) and
          (dispatchPipelineReg[j][0] == 1) and
          (dispatchPipelineReg[j][4] == 1))
      {
        dispatchPipelineReg[j][5] = 1;
        dispatchPipelineReg[j][4] = 0;
      }
      if ((dispatchPipelineReg[j][9] == robHeadPtr) and
          (dispatchPipelineReg[j][0] == 1) and
          (dispatchPipelineReg[j][7] == 1))
      {
        dispatchPipelineReg[j][8] = 1;
        dispatchPipelineReg[j][7] = 0;
      }
    }

    for (size_t j = 0; j < params.width; j++)
    {
      if ((regreadPipelineReg[j][5] == robHeadPtr) and
          (regreadPipelineReg[j][0] == 1) and
          (regreadPipelineReg[j][4] == 1))
      {
        regreadPipelineReg[j][4] = 0;
      }
      if ((regreadPipelineReg[j][7] == robHeadPtr) and
          (regreadPipelineReg[j][0] == 1) and
          (regreadPipelineReg[j][6] == 1))
      {
        regreadPipelineReg[j][6] = 0;
      }
    }
    robHeadPtr = (robHeadPtr + 1) % params.rob_size;
  }
}

void Writeback()
{

  // Process the writeback bundle in WB:
  // For each instruction in WB, mark the
  // instruction as “ready” in its entry in
  // the ROB.

  for (size_t i = 0; i < params.width * 5; i++)
  {
    if (writebackPipelineReg[i][0] == 1)
    {
      ROB[writebackPipelineReg[i][1]][1] = 1;
      writebackPipelineReg[i][0] = 0;
      instructionsVector[writebackPipelineReg[i][2]][13] = simCycle;
    }
  }
}

void Execute()
{
  // From the execute_list, check for
  // instructions that are finishing
  // execution this cycle, and:
  // 1) Remove the instruction from
  // the execute_list.
  // 2) Add the instruction to WB.
  // 3) Wakeup dependent instructions (set
  // their source operand ready flags) in
  // the IQ, DI (the dispatch bundle), and
  // RR (the register-read bundle).

  // execution manager -- to track latency
  for (size_t i = 0; i < params.width * 5; i++)
  {
    if (executePipelineReg[i][0])
      executePipelineReg[i][2]--;
  }

  wakeupVector.clear();

  // if latency is 0, exec done
  for (size_t i = 0; i < params.width * 5; i++)
  {
    if (executePipelineReg[i][0] and
        (executePipelineReg[i][2] == 0))
    {
      for (size_t k = 0; k < params.width * 5; k++)
      {
        if (writebackPipelineReg[k][0] == 0)
        {
          writebackPipelineReg[k][0] = 1;
          writebackPipelineReg[k][1] = executePipelineReg[i][1];
          writebackPipelineReg[k][2] = executePipelineReg[i][3];
          executePipelineReg[i][0] = 0;
          wakeupVector.push_back(executePipelineReg[i][1]);
          instructionsVector[executePipelineReg[i][3]][12] = simCycle;
          break;
        }
      }
    }
  }
}

void Issue()
{
  // Issue up to WIDTH oldest instructions
  // from the IQ. (One approach to implement
  // oldest-first issuing, is to make multiple
  // passes through the IQ, each time finding
  // the next oldest ready instruction and
  // then issuing it. One way to annotate the
  // age of an instruction is to assign an
  // incrementing sequence number to each
  // instruction as it is fetched from the
  // trace file.)
  // To issue an instruction:
  // 1) Remove the instruction from the IQ.
  // 2) Add the instruction to the
  // execute_list. Set a timer for the
  // instruction in the execute_list that
  // will allow you to model its execution
  // latency.

  int priorityInstr = -1;
  int toBeExec = 0;
  int priorityInstrUniqVal = INT_MAX;

  for (size_t i = 0; i < params.width; i++)
  {
    for (size_t j = 0; j < params.iq_size; j++)
    {
      for (size_t k = 0; k < wakeupVector.size(); k++)
      {
        if ((issueQueueVector[j][4] == wakeupVector[k]) and
            (issueQueueVector[j][0] == 1))
        {
          issueQueueVector[j][3] = 1;
        }
        if ((issueQueueVector[j][6] == wakeupVector[k]) and
            (issueQueueVector[j][0] == 1))
        {
          issueQueueVector[j][5] = 1;
        }
      }

      if ((issueQueueVector[j][7] < priorityInstrUniqVal) and
          issueQueueVector[j][0] and issueQueueVector[j][3] and
          issueQueueVector[j][5])
      {
        priorityInstr = j;
        priorityInstrUniqVal = issueQueueVector[j][7];
      }
    }

    if (priorityInstr == -1)
      return;
    instructionsVector[issueQueueVector[priorityInstr][7]][11] = simCycle;

    for (size_t j = 0; j < params.width * 5; j++)
    {

      if (executePipelineReg[j][0] == 0)
      {
        toBeExec = j;
        break;
      }
    }
    executePipelineReg[toBeExec][0] = 1;
    executePipelineReg[toBeExec][3] =
        issueQueueVector[priorityInstr][7];
    executePipelineReg[toBeExec][1] =
        issueQueueVector[priorityInstr][1];

    executePipelineReg[toBeExec][2] =
        getTimeForOp(issueQueueVector[priorityInstr][2]);
    priorityInstrUniqVal = INT_MAX;
    issueQueueVector[priorityInstr][0] =
        0;
  }
}

void Dispatch()
{
  // If DI contains a dispatch bundle:
  // If the number of free IQ entries is less
  // than the size of the dispatch bundle in
  // DI, then do nothing. If the number of
  // free IQ entries is greater than or equal
  // to the size of the dispatch bundle in DI,
  // then dispatch all instructions from DI to
  // the IQ

  unsigned int issueQCount = 0, instrCount = 0;

  instrCount =
      count_if(dispatchPipelineReg.begin(), dispatchPipelineReg.end(), bundleCount);

  issueQCount =
      count_if(issueQueueVector.begin(), issueQueueVector.end(), bundleCount);

  unsigned long long int remaining_q = params.iq_size - issueQCount;
  if (instrCount > remaining_q)
    return;
  if (instrCount > 0)
  {
    for (size_t i = 0; i < params.width; i++)
    {
      if (dispatchPipelineReg[i][0])
      {
        for (size_t j = 0; j < params.iq_size; j++)
        {
          for (size_t k = 0; k < wakeupVector.size(); k++)
          {
            if ((dispatchPipelineReg[i][6] == wakeupVector[k]) and
                (dispatchPipelineReg[i][0] == 1) and
                (dispatchPipelineReg[i][4] == 1))
            {
              dispatchPipelineReg[i][5] = 1;
              dispatchPipelineReg[i][4] = 0;
            }
            if ((dispatchPipelineReg[i][9] == wakeupVector[k]) and
                (dispatchPipelineReg[i][0] == 1) and
                (dispatchPipelineReg[i][7] == 1))
            {
              dispatchPipelineReg[i][8] = 1;
              dispatchPipelineReg[i][7] = 0;
            }
          }

          if (!issueQueueVector[j][0])
          {
            issueQueueVector[j][0] = 1;
            issueQueueVector[j][1] = dispatchPipelineReg[i][3];
            issueQueueVector[j][3] = dispatchPipelineReg[i][5];

            if (dispatchPipelineReg[i][6] >= 0)
            {
              if (dispatchPipelineReg[i][4])
              {
                issueQueueVector[j][3] =
                    ROB[dispatchPipelineReg[i][6]]
                       [1];
              }
              else
              {
                issueQueueVector[j][3] = 1;
              }
            }
            else
            {
              issueQueueVector[j][3] = 1;
            }
            issueQueueVector[j][4] = dispatchPipelineReg[i][6];

            if (dispatchPipelineReg[i][9] >= 0)
            {
              if (dispatchPipelineReg[i][7])
              {
                issueQueueVector[j][5] =
                    ROB[dispatchPipelineReg[i][9]]
                       [1];
              }
              else
              {
                issueQueueVector[j][5] = 1;
              }
            }
            else
            {
              issueQueueVector[j][5] = 1;
            }
            issueQueueVector[j][6] = dispatchPipelineReg[i][9];
            issueQueueVector[j][7] = dispatchPipelineReg[i][10];
            issueQueueVector[j][2] = dispatchPipelineReg[i][2];
            break;
          }
        }
        instructionsVector[dispatchPipelineReg[i][10]][10] = simCycle;
      }
    }

    invalidateBundle(dispatchPipelineReg);
  }
}

void RegRead()
{

  // If RR contains a register-read bundle:
  // If DI is not empty (cannot accept a
  // new dispatch bundle), then do nothing.
  // If DI is empty (can accept a new dispatch
  // bundle), then process (see below) the
  // register-read bundle and advance it from
  // RR to DI.
  //
  // Since values are not explicitly modeled,
  // the sole purpose of the Register Read
  // stage is to ascertain the readiness of
  // the renamed source operands.

  if (bundleExists(regreadPipelineReg) and !bundleExists(dispatchPipelineReg))
  {
    for (size_t i = 0; i < params.width; i++)
    {
      if (regreadPipelineReg[i][0])
      {
        for (size_t j = 0; j < 4; j++)
        {
          dispatchPipelineReg[i][j] = regreadPipelineReg[i][j];
        }
        if (regreadPipelineReg[i][5] >= 0)
        {
          // rob
          if (regreadPipelineReg[i][4])
          {
            // rob entry ready
            dispatchPipelineReg[i][5] =
                ROB[regreadPipelineReg[i][5]][1];
            dispatchPipelineReg[i][4] =
                bool(!(ROB[regreadPipelineReg[i][5]][1]));
          }
          // arf or imm val; set rdy bit
          else
          {
            dispatchPipelineReg[i][5] = 1;
            dispatchPipelineReg[i][4] = 0;
          }
        }
        // imm val, set rdy bit
        else
        {
          dispatchPipelineReg[i][5] = 1;
          dispatchPipelineReg[i][4] = 0;
        }
        dispatchPipelineReg[i][6] = regreadPipelineReg[i][5];

        // src2 is rob_reg. if rdy
        if (regreadPipelineReg[i][7] >= 0)
        {
          if (regreadPipelineReg[i][6])
          {
            // src2 is rdy/not in rob, change dispatch rdy values approp.
            dispatchPipelineReg[i][8] =
                ROB[regreadPipelineReg[i][7]][1];
            dispatchPipelineReg[i][7] =
                !ROB[regreadPipelineReg[i][7]][1];
          }
          // arf or imm val;
          else
          {
            dispatchPipelineReg[i][8] = 1;
            dispatchPipelineReg[i][7] = 0;
          }
        }
        // imm val; set rdy bit and set rob entry flag
        else
        {
          dispatchPipelineReg[i][8] = 1;
          dispatchPipelineReg[i][7] = 0;
        }
        dispatchPipelineReg[i][9] = regreadPipelineReg[i][7];

        dispatchPipelineReg[i][10] = regreadPipelineReg[i][8];

        instructionsVector[regreadPipelineReg[i][8]][9] = simCycle;
      }

      for (size_t k = 0; k < wakeupVector.size(); k++)
      {
        if ((regreadPipelineReg[i][5] == wakeupVector[k]) and
            (regreadPipelineReg[i][0] == 1) and
            (regreadPipelineReg[i][4] == 1))
        {
          regreadPipelineReg[i][4] = 0;
        }
        if ((regreadPipelineReg[i][7] == wakeupVector[k]) and
            (regreadPipelineReg[i][0] == 1) and
            (regreadPipelineReg[i][6] == 1))
        {
          regreadPipelineReg[i][6] = 0;
        }
      }
    }
    invalidateBundle(regreadPipelineReg);
  }
}

void Rename()
{
  // If RN contains a rename bundle:
  // If either RR is not empty (cannot accept
  // a new register-read bundle) or the ROB
  // does not have enough free entries to
  // accept the entire rename bundle, then do
  // nothing.
  // If RR is empty (can accept a new
  // register-read bundle) and the ROB has
  // enough free entries to accept the entire
  // rename bundle, then process (see below)
  // the rename bundle and advance it from
  // RN to RR.
  //
  // (1) allocate an entry in the ROB for the
  // instruction, (2) rename its source
  // registers, and (3) rename its destination
  // register (if it has one).

  int instrCount = 0;

  instrCount =
      count_if(renamePipelineReg.begin(), renamePipelineReg.end(), bundleCount);

  if ((instrCount > 0) and !bundleExists(regreadPipelineReg))
  {
    int robSizeUsed = robTailPtr - robHeadPtr;
    if (robSizeUsed > 0 and (params.rob_size - robSizeUsed) < instrCount)
    {
      return;
    }
    if (robSizeUsed == 0 and priorFetch)
      return;
    if (robSizeUsed < 0 and -1 * (robSizeUsed) < instrCount)
      return;
    for (size_t i = 0; i < params.width; i++)
    {
      if (renamePipelineReg[i][0])
      {
        if (renamePipelineReg[i][4] >= 0)
        {
          if (RMT[renamePipelineReg[i][4]].first)
          {
            regreadPipelineReg[i][4] = 1;
            regreadPipelineReg[i][5] =
                RMT[renamePipelineReg[i][4]].second;
          }
          else
          {
            regreadPipelineReg[i][4] = 0;
            regreadPipelineReg[i][5] = renamePipelineReg[i][4];
          }
        }

        else
        {
          regreadPipelineReg[i][4] = 0;
          regreadPipelineReg[i][5] = renamePipelineReg[i][4];
        }
        if (renamePipelineReg[i][5] >= 0)
        {
          if (RMT[renamePipelineReg[i][5]].first)
          {
            regreadPipelineReg[i][6] = 1;
            regreadPipelineReg[i][7] =
                RMT[renamePipelineReg[i][5]].second;
          }
          else
          {
            regreadPipelineReg[i][6] = 0;
            regreadPipelineReg[i][7] = renamePipelineReg[i][5];
          }
        }

        else
        {
          regreadPipelineReg[i][6] = 0;
          regreadPipelineReg[i][7] = renamePipelineReg[i][5];
        }

        if (renamePipelineReg[i][3] >= 0)
        {
          RMT[renamePipelineReg[i][3]] = {true, robTailPtr};
        }

        regreadPipelineReg[i][3] = robTailPtr;

        priorFetch = true;

        ROB[robTailPtr] = {
            renamePipelineReg[i][3], 0, 0, 0, renamePipelineReg[i][1],
            renamePipelineReg[i][6]};

        robTailPtr = (params.rob_size + robTailPtr + 1) % params.rob_size;

        for (int j = 0; j < 3; j++)
        {
          regreadPipelineReg[i][j] = renamePipelineReg[i][j];
        }
        regreadPipelineReg[i][8] = renamePipelineReg[i][6];
        instructionsVector[renamePipelineReg[i][6]][8] = simCycle;
      }
    }

    invalidateBundle(renamePipelineReg);
  }
}

void Decode()
{

  // If DE contains a decode bundle:
  // If RN is not empty (cannot accept a new
  // rename bundle), then do nothing.
  // If RN is empty (can accept a new rename
  // bundle), then advance the decode bundle
  // from DE to RN.

  if (bundleExists(decodePipelineReg) and !bundleExists(renamePipelineReg))
  {
    for (size_t i = 0; i < params.width; i++)
    {
      if (decodePipelineReg[i][0])
      {
        renamePipelineReg[i] = decodePipelineReg[i];
        instructionsVector[decodePipelineReg[i][6]][7] = simCycle;
      }
    }
    invalidateBundle(decodePipelineReg);
  }
}

void Fetch()
{

  /*
  // Do nothing if either (1) there are no
  // more instructions in the trace file or
  // (2) DE is not empty (cannot accept a new
  // decode bundle).
  //
  // If there are more instructions in the
  // trace file and if DE is empty (can accept
  // a new decode bundle), then fetch up to
  // WIDTH instructions from the trace file
  // into DE. Fewer than WIDTH instructions
  // will be fetched only if the trace file
  // has fewer than WIDTH instructions left.
  */

  if (bundleExists(fetchPipelineReg) and !bundleExists(decodePipelineReg))
  {
    for (size_t i = 0; i < params.width; i++)
    {
      if (fetchPipelineReg[i][0])
      {
        decodePipelineReg[i] = fetchPipelineReg[i];
        instructionsVector[fetchPipelineReg[i][6]][5] = simCycle - 1;
        instructionsVector[fetchPipelineReg[i][6]][6] = simCycle;
      }
    }
    invalidateBundle(fetchPipelineReg);
  }

  for (size_t i = 0; i < params.width; i++)
  {
    if (fetchPipelineReg[i][0])
      return;
  }

  for (size_t i = 0; i < params.width; i++)
  {
    tracesDone = fscanf(FP, "%lx %d %d %d %d", &pc, &op_type, &dest,
                        &src1, &src2) == EOF;
    if (!tracesDone)
    {
      vector<int> tempInstr = vector<int>(15);
      tempInstr[0] = uniqueVal;
      tempInstr[1] = op_type;
      tempInstr[2] = src1;
      tempInstr[3] = src2;
      tempInstr[4] = dest;
      instructionsVector.push_back(tempInstr);
      fetchPipelineReg[i] = {1, pc, op_type, dest, src1, src2, uniqueVal};
      uniqueVal++;
    }
  }
}

bool Advance_Cycle()
{
  bool bundleExistsFlag = false;
  bundleExistsFlag |= bundleExists(decodePipelineReg) or
                      bundleExists(dispatchPipelineReg) or bundleExists(regreadPipelineReg);
  bundleExistsFlag |= bundleExists(renamePipelineReg) or
                      bundleExists(writebackPipelineReg) or bundleExists(executePipelineReg);
  bundleExistsFlag |= bundleExists(issueQueueVector);
  bundleExistsFlag |= (robHeadPtr != robTailPtr);
  simCycle++;

  if (tracesDone and !bundleExistsFlag)
    return false;
  return true;
}

void print(char *argv[])
{

  // 0 fu{0} src{29,14} dst{-1} FE{0,1} DE{1,1} RN{2,1} RR{3,1} DI{4,1} IS{5,1} EX{6,1} WB{7,1} RT{8,1}

  for (size_t i = 0; i < instructionsVector.size(); i++)
  {
    // start sim - end sim

    cout << instructionsVector[i][0]
         << " fu{" << instructionsVector[i][1]
         << "} src{" << instructionsVector[i][2] << "," << instructionsVector[i][3]
         << "} dst{" << instructionsVector[i][4]
         << "} FE{" << instructionsVector[i][5] << "," << instructionsVector[i][6] - instructionsVector[i][5]
         << "} DE{" << instructionsVector[i][6] << "," << instructionsVector[i][7] - instructionsVector[i][6]
         << "} RN{" << instructionsVector[i][7] << "," << instructionsVector[i][8] - instructionsVector[i][7]
         << "} RR{" << instructionsVector[i][8] << "," << instructionsVector[i][9] - instructionsVector[i][8]
         << "} DI{" << instructionsVector[i][9] << "," << instructionsVector[i][10] - instructionsVector[i][9]
         << "} IS{" << instructionsVector[i][10] << "," << instructionsVector[i][11] - instructionsVector[i][10]
         << "} EX{" << instructionsVector[i][11] << "," << instructionsVector[i][12] - instructionsVector[i][11]
         << "} WB{" << instructionsVector[i][12] << "," << instructionsVector[i][13] - instructionsVector[i][12]
         << "} RT{" << instructionsVector[i][13] << "," << instructionsVector[i][14] - instructionsVector[i][13] << "}\n";
  }
  cout << "# === Simulator Command ========="
       << "\n# " << argv[0] << " " << argv[1] << " " << argv[2] << " " << argv[3] << " " << argv[4]
       << "\n# === Processor Configuration ==="
       << "\n# ROB_SIZE = " << argv[1]
       << "\n# IQ_SIZE  = " << argv[2]
       << "\n# WIDTH    = " << argv[3]
       << "\n# === Simulation Results ========"
       << "\n# Dynamic Instruction Count    = " << uniqueVal
       << "\n# Cycles                       = " << simCycle - 1
       << "\n# Instructions Per Cycle (IPC) = " << fixed << setprecision(2)
       << double(uniqueVal) / double(simCycle - 1);
}

#endif
