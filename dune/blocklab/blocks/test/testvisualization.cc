#include"config.h"

#include<dune/blocklab/blocks/visualization.hh>
#include<dune/blocklab/blocks/test/providersetup.hh>
#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/parametertree.hh>
#include<dune/common/test/testsuite.hh>

#include<memory>
#include<tuple>


int main(int argc, char** argv)
{
  Dune::MPIHelper& helper = Dune::MPIHelper::instance(argc, argv);
  Dune::TestSuite test;
  Dune::ParameterTree config;

  config["solver.blocks"] = "visualization";
  config["visualization.blocks"] = "vis_vector, vis_mpirank, vis_indexset";
  config["visualization.filename"] = "vtktest";

  auto ctx = structured_ug2_p1fem(helper, config);
  ctx.template registerBlock<Dune::BlockLab::VisualizationBlock>("visualization");
  ctx.template registerBlock<Dune::BlockLab::IndexSetVisualizationBlock>("vis_indexset");
  ctx.template registerBlock<Dune::BlockLab::MPIRankVisualizationBlock>("vis_mpirank");
  ctx.template registerBlock<Dune::BlockLab::VectorVisualizationBlock>("vis_vector");

  auto solver = ctx.constructSolver(config.sub("solver"));
  solver->apply();

  return test.exit();
}
