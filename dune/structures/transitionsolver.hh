#ifndef DUNE_STRUCTURES_TRANSITIONSOLVER_HH
#define DUNE_STRUCTURES_TRANSITIONSOLVER_HH

/**
 *  A nonlinear solver that solves for a sequence of problems
 *  of increasing physical complexity
 */
#include<dune/common/shared_ptr.hh>
#include<dune/pdelab.hh>
#include<dune/structures/utilities.hh>

// Include all available solver steps in order to have this header serve
// as a convenience header
#include<dune/structures/solversteps/base.hh>
#include<dune/structures/solversteps/constraints.hh>
#include<dune/structures/solversteps/elasticity.hh>
#include<dune/structures/solversteps/interpolation.hh>
#include<dune/structures/solversteps/material.hh>
#include<dune/structures/solversteps/newton.hh>
#include<dune/structures/solversteps/onetoone.hh>
#include<dune/structures/solversteps/probe.hh>
#include<dune/structures/solversteps/transformation.hh>
#include<dune/structures/solversteps/variation.hh>
#include<dune/structures/solversteps/visualization.hh>

#include<memory>
#include<vector>


template<typename Vector>
class TransitionSolver
{
  public:
  using ConstraintsContainer = typename Vector::GridFunctionSpace::template ConstraintsContainer<typename Vector::field_type>::Type;

  TransitionSolver()
    : steps(0)
  {}

  TransitionSolver(std::vector<std::shared_ptr<TransitionSolverStepBase<Vector>>> steps)
    : steps(steps)
  {}

  template<typename STEP>
  void add(std::shared_ptr<STEP> step)
  {
    steps.push_back(step);
  }

  template<typename STEP>
  void add(STEP& step)
  {
    steps.push_back(Dune::stackobject_to_shared_ptr(step));
  }

  void apply(std::shared_ptr<Vector> vector, std::shared_ptr<ConstraintsContainer> constraintscontainer)
  {
    for (auto step : steps)
      step->pre(vector, constraintscontainer);

    for (auto step : steps)
      step->apply(vector, constraintscontainer);

    for (auto step : steps)
     step->post(vector, constraintscontainer);
  }

  private:
  std::vector<std::shared_ptr<TransitionSolverStepBase<Vector>>> steps;
};

#endif
