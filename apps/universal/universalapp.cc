#include"config.h"

#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/parametertree.hh>
#include<dune/structures/elasticity.hh>
#include<dune/structures/gridconstruction.hh>
#include<dune/structures/gridreorder.hh>
#include<dune/structures/solverconstruction.hh>

#include<memory>
#include<tuple>


template<int dim, int degree>
void apply(Dune::MPIHelper& helper, const Dune::ParameterTree& params, char** argv)
{
  auto [grid, es, physical] = construct_grid<dim>(helper, params.sub("grid"), argv);
  std::tie(grid, es, physical) = reorder_grid(grid, es, physical, params.sub("grid"));
  auto [x, cc, force, force_cc, traction, traction_cc] = elasticity_setup<degree>(es);
  using V = typename std::remove_reference<decltype(*x)>::type;
  using V_F = typename std::remove_reference<decltype(*force)>::type;
  using V_T = typename std::remove_reference<decltype(*traction)>::type;

  ConstructionContext<V, V_F, V_T> ctx(helper, params, grid, es, physical);
  ctx.setVectors(x, force, traction);
  ctx.setConstraintsContainers(cc, force_cc, traction_cc);

  auto solver = ctx.construct(params.sub("solver"));

  solver->apply();
}


int main(int argc, char** argv)
{
  Dune::MPIHelper& helper = Dune::MPIHelper::instance(argc, argv);

  // Parse the ini file
  Dune::ParameterTree params;
  Dune::ParameterTreeParser::readOptions(argc, argv, params);
  Dune::ParameterTreeParser::readINITree(argv[1], params, false);

  // Dispatch on grid dimension
  auto dim = params.get<int>("grid.dimension", 3);
  auto degree = params.get<int>("solver.degree", 1);

  if (dim == 2)
  {
    if (degree == 1)
      apply<2, 1>(helper, params, argv);
    if (degree == 2)
      apply<2, 2>(helper, params, argv);
  }
  if (dim == 3)
  {
    if (degree == 1)
      apply<3, 1>(helper, params, argv);
    if (degree == 2)
      apply<3, 2>(helper, params, argv);
  }
  return 0;
}
