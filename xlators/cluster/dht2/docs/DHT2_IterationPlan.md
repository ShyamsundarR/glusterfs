# DHT2: Iteration plan

Each iteration is planned for 3 weeks.

## Iteration 1
Starts:   Aug-10-2015
Ends:     Aug-28-2015

Goal:
  - Prototype initial design as detailed in
    xlators/cluster/dht2/docs/DHT2_FirstPrototypeDesign.md
  - Mature definition of next design, to overcome limitations of the first

Tasks:
  - POSIX layer needs a rewrite based on new on disk format of data
    - Indivudial FOPs can be divided or take up by people interested, once 2-3
      FOPs are implemented, as they would server as a template for others
  - DHT2 first prototype FOPs need to be implemented
    - Indivudial FOPs can be divided or take up by people interested, once 2-3
      FOPs are implemented, as they would server as a template for others
  - DHT2 second prototype design 70% complete

Notes:
  - Prototype intended is for throwaway purposes, hence will not meet coding
    or unit and functional testing standards

## Iteration 2
Starts: Aug-31-2015
Ends:   Sep-18-2015

Goal:
  - Complete initial design prototype (or, as much as needed)
    - This is a decision point, if next design is chosen, the first prototype
      would be closed at a functional point that can enable further
      experimentation with it by others
  - Complete next phase of design
  - Complete next prototype implementation at least 50%
  - Identify major areas that need attention to productize the design based on
    the prototype

Tasks:
  - <TBD> Will be filled as I1 is being closed out

Notes:
  - Prototype intended is for throwaway purposes, hence will not meet coding
    or unit and functional testing standards

## Iteration 3
Starts: Sep-21-2015
Ends:   Oct-09-2015

Goal:
  - Finalize design
  - Finalize cross functional requirements
  - Finalize cross component changes
  - Finalize productization requirements
    - Testing methodologies (model checker, performance, scale,
      unit/functional tests), documentation, others
  - Get productization efforts operationally ready
    - Branch creation and contribution/review workflow etc.

Tasks:
  - <TBD> Will be filled as I2 is being closed out

Notes:
  - <NA>

## Further iterations TBD

