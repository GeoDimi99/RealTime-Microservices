# Spacecraft Schedule Timeline Visualization

## 📊 Timeline Overview (0-117 ms)

```
Time (ms): 0    10   20   30   40   50   60   70   80   90   100  110  120
           |----|----|----|----|----|----|----|----|----|----|----|----|
           
AOCN_CMG   [==]                    [==]
(P:134)    15-31                   62-93

SATCM                         [==]
(P:136)                       39-54

ORB                                     [===]
(P:136)                                 62-93

SWAPF           [================================]
(P:137)         23-----------------------------102

SSMAPF                   [=======================]
(P:138)                  46--------------------102

MHSTR                         [======================]
(P:139)                       39------------------93

SFDIR                                                    [=]
(P:139)                                                101-109

AOCN                                        [=============]
(P:141)                                     70---------117

AOCSCM_POST                                 [========]
(P:142)                                     70------102

CMG        [==]                    [===]
(P:143)    15-31                   62-93
```

## 📈 Task Execution Order (by Start Time)

| Start (ms) | Task(s)                    | Priority | Duration (ms) |
|-----------|----------------------------|----------|---------------|
| 15        | CMG, AOCN_CMG             | 143, 134 | 16, 16        |
| 23        | SWAPF                     | 137      | 79            |
| 39        | SATCM, MHSTR              | 136, 139 | 15, 54        |
| 46        | SSMAPF                    | 138      | 56            |
| 62        | CMG, AOCN_CMG, ORB        | 143, 134, 136 | 31, 31, 31 |
| 70        | AOCN, AOCSCM_POST         | 141, 142 | 47, 32        |
| 101       | SFDIR                     | 139      | 8             |

## 🔥 Critical Time Windows

### Window 1: 15-31 ms
**Active Tasks**: 2
- ⚡ AOCN_CMG (Priority 134 - HIGHEST)
- CMG (Priority 143)

### Window 2: 23-39 ms
**Active Tasks**: 1
- SWAPF (Priority 137)

### Window 3: 39-46 ms
**Active Tasks**: 3
- SWAPF (Priority 137)
- SATCM (Priority 136)
- MHSTR (Priority 139)

### Window 4: 46-54 ms
**Active Tasks**: 4
- SWAPF (Priority 137)
- SSMAPF (Priority 138)
- SATCM (Priority 136)
- MHSTR (Priority 139)

### Window 5: 62-70 ms (CRITICAL - 5 tasks!)
**Active Tasks**: 5
- ⚡ AOCN_CMG (Priority 134 - HIGHEST)
- ORB (Priority 136)
- SWAPF (Priority 137)
- SSMAPF (Priority 138)
- MHSTR (Priority 139)
- CMG (Priority 143)

### Window 6: 70-93 ms (CRITICAL - 6 tasks!)
**Active Tasks**: 6
- ⚡ AOCN_CMG (Priority 134 - HIGHEST)
- ORB (Priority 136)
- SWAPF (Priority 137)
- SSMAPF (Priority 138)
- MHSTR (Priority 139)
- AOCN (Priority 141)
- AOCSCM_POST (Priority 142)
- CMG (Priority 143)

### Window 7: 93-101 ms
**Active Tasks**: 3
- SWAPF (Priority 137)
- SSMAPF (Priority 138)
- AOCN (Priority 141)
- AOCSCM_POST (Priority 142)

### Window 8: 101-102 ms
**Active Tasks**: 4
- SWAPF (Priority 137)
- SSMAPF (Priority 138)
- AOCN (Priority 141)
- SFDIR (Priority 139)

### Window 9: 102-109 ms
**Active Tasks**: 2
- AOCN (Priority 141)
- SFDIR (Priority 139)

### Window 10: 109-117 ms
**Active Tasks**: 1
- AOCN (Priority 141)

## ⚠️ Scheduling Challenges

### 1. High Concurrency Window (62-93 ms)
- **Up to 6-8 tasks** active simultaneously
- **Critical for scheduling**: Need efficient priority-based preemption
- **Risk**: Deadline misses if not properly scheduled

### 2. Recurring Tasks
- **CMG**: Executes at 15ms and 62ms
- **AOCN_CMG**: Executes at 15ms and 62ms
- Both have different priorities but same execution windows

### 3. Long-Running Tasks
- **SWAPF**: 79ms duration (23-102ms)
- **SSMAPF**: 56ms duration (46-102ms)
- **MHSTR**: 54ms duration (39-93ms)
- **AOCN**: 47ms duration (70-117ms)

## 🎯 Priority Hierarchy

```
Priority 134 (HIGHEST) → AOCN_CMG
Priority 136           → SATCM, ORB
Priority 137           → SWAPF
Priority 138           → SSMAPF
Priority 139           → MHSTR, SFDIR
Priority 141           → AOCN
Priority 142           → AOCSCM_POST
Priority 143 (LOWEST)  → CMG
```

## 📊 Task Statistics

| Metric                    | Value |
|---------------------------|-------|
| Total unique tasks        | 10    |
| Total task instances      | 12    |
| Schedule duration         | 117 ms|
| Max concurrent tasks      | 6-8   |
| Recurring tasks           | 2 (CMG, AOCN_CMG) |
| Average task duration     | ~34 ms|
| Shortest task             | SFDIR (8 ms) |
| Longest task              | SWAPF (79 ms) |

## 🔧 Recommended Scheduling Strategy

1. **Use Priority-Based Preemptive Scheduling**
   - Respect priority levels (134 = highest)
   - Allow preemption when higher priority task arrives

2. **Monitor Critical Windows**
   - Window 62-93 ms: Maximum load
   - Ensure AOCN_CMG (P:134) gets CPU when needed

3. **Consider Rate Monotonic Analysis**
   - Shorter periods → Higher priority
   - Validate schedulability

4. **Implement Deadline Monitoring**
   - Track deadline misses
   - Log performance metrics

## 📝 Notes

- All times are in **milliseconds**
- Priorities: **Lower number = Higher priority**
- Policy: Currently all FIFO, consider EDF for better deadline guarantees
- Some tasks may need adjustment based on actual execution times
