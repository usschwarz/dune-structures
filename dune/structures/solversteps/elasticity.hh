#ifndef DUNE_STRUCTURES_SOLVERSTEPS_ELASTICITY_HH
#define DUNE_STRUCTURES_SOLVERSTEPS_ELASTICITY_HH

#include<dune/structures/solversteps/base.hh>
#include<dune/structures/solversteps/instationary.hh>
#include<dune/structures/solversteps/material.hh>
#include<dune/structures/solversteps/newton.hh>

#include"operators/elasticity_operator.hh"
#include"operators/elastodynamics_spatial_operator.hh"
#include"operators/elastodynamics_temporal_operator.hh"


template<typename Vector>
class ElasticitySolverStep
  : public MaterialDependantStepBase<Vector>
{
  public:
  using Base = TransitionSolverStepBase<Vector>;
  using LocalOperator = ElasticityOperator<typename TransitionSolverStepBase<Vector>::GridFunctionSpace,
                                           typename TransitionSolverStepBase<Vector>::GridFunctionSpace>;

  ElasticitySolverStep(typename Base::EntitySet es,
                       std::shared_ptr<std::vector<int>> physical,
                       const Dune::ParameterTree& rootparams
                       )
    : MaterialDependantStepBase<Vector>(es, physical, rootparams.sub("material"))
    , newton_step(std::make_shared<NewtonSolverTransitionStep<Vector, LocalOperator>>())
    , params(rootparams)
  {}

  virtual ~ElasticitySolverStep() {}

  virtual void pre(std::shared_ptr<Vector> vector, std::shared_ptr<typename Base::ConstraintsContainer> cc) override
  {
    auto& gfs = vector->gridFunctionSpace();
    newton_step->set_localoperator(std::make_shared<LocalOperator>(gfs, gfs, params, this->material));
    newton_step->pre(vector, cc);
  }

  virtual void apply(std::shared_ptr<Vector> vector, std::shared_ptr<typename Base::ConstraintsContainer> cc) override
  {
    newton_step->apply(vector, cc);
  }

  virtual void post(std::shared_ptr<Vector> vector, std::shared_ptr<typename Base::ConstraintsContainer> cc) override
  {
    newton_step->post(vector, cc);
  }

  private:
  std::shared_ptr<NewtonSolverTransitionStep<Vector, LocalOperator>> newton_step;
  Dune::ParameterTree params;
};


template<typename Vector>
class ElastoDynamicsSolverStep
  : public MaterialDependantStepBase<Vector>
{
  public:
  using Base = TransitionSolverStepBase<Vector>;

  using SpatialLocalOperator = ElastoDynamicsSpatialOperator<typename TransitionSolverStepBase<Vector>::GridFunctionSpace,
                                                             typename TransitionSolverStepBase<Vector>::GridFunctionSpace>;

  using TemporalLocalOperator = ElastoDynamicsTemporalOperator<typename TransitionSolverStepBase<Vector>::GridFunctionSpace,
                                                               typename TransitionSolverStepBase<Vector>::GridFunctionSpace>;

  ElastoDynamicsSolverStep(typename Base::EntitySet es,
                           std::shared_ptr<std::vector<int>> physical,
                           const Dune::ParameterTree& rootparams)
    : MaterialDependantStepBase<Vector>(es, physical, rootparams.sub("material"))
    , onestepmethod_step(std::make_shared<OneStepMethodStep<Vector, SpatialLocalOperator, TemporalLocalOperator>>())
    , params(rootparams)
  {}

  virtual ~ElastoDynamicsSolverStep() {}

  virtual void pre(std::shared_ptr<Vector> vector, std::shared_ptr<typename Base::ConstraintsContainer> cc) override
  {
    auto& gfs = vector->gridFunctionSpace();
    onestepmethod_step->set_spatial_localoperator(std::make_shared<SpatialLocalOperator>(gfs, gfs, params, this->material));
    onestepmethod_step->set_temporal_localoperator(std::make_shared<TemporalLocalOperator>(gfs, gfs, params));
    onestepmethod_step->pre(vector, cc);
  }

  virtual void apply(std::shared_ptr<Vector> vector, std::shared_ptr<typename Base::ConstraintsContainer> cc) override
  {
    onestepmethod_step->apply(vector, cc);
  }

  virtual void post(std::shared_ptr<Vector> vector, std::shared_ptr<typename Base::ConstraintsContainer> cc) override
  {
    onestepmethod_step->post(vector, cc);
  }

  private:
  std::shared_ptr<OneStepMethodStep<Vector, SpatialLocalOperator, TemporalLocalOperator>> onestepmethod_step;
  Dune::ParameterTree params;
};
#endif
