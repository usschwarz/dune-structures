#ifndef DUNE_STRUCTURES_SOLVERSTEPS_VISUALIZATION_HH
#define DUNE_STRUCTURES_SOLVERSTEPS_VISUALIZATION_HH

#include<dune/common/shared_ptr.hh>
#include<dune/grid/io/file/vtk/vtkwriter.hh>
#include<dune/pdelab.hh>
#include<dune/structures/material.hh>
#include<dune/structures/solversteps/base.hh>
#include<dune/structures/solversteps/traits.hh>
#include<dune/structures/vonmises.hh>

#include<filesystem>
#include<memory>
#include<vector>
#include<variant>


template<typename GV, bool instationary>
struct VTKWriterChooser
{
  using type = Dune::VTKWriter<GV>;
  using ptype = std::shared_ptr<type>;
};


template<typename GV>
struct VTKWriterChooser<GV, true>
{
  using type = Dune::VTKSequenceWriter<GV>;
  using ptype = std::shared_ptr<type>;
};


template<typename... V>
class VisualizationStep;


template<typename... V>
class VisualizationStepBase
  : public TransitionSolverStepBase<V...>
{
  public:
  using Traits = SimpleStepTraits<V...>;

  void set_parent(VisualizationStep<V...>* parent_)
  {
    parent = parent_;
  }

  protected:
  VisualizationStep<V...>* parent;
};


template<typename... V>
class VisualizationStep
  : public StepCollectionStep<V...>
{
  public:
  using Traits = SimpleStepTraits<V...>;

  using VTKWriter = std::variant<
      std::shared_ptr<typename VTKWriterChooser<typename Traits::GridView, true>::type>,
      std::shared_ptr<typename VTKWriterChooser<typename Traits::GridView, false>::type>>;

  VisualizationStep(bool instationary = false,
                    std::string name="output")
    : instationary(instationary), time(0.0), name(name), path(""), extendpath("")
  {}

  VisualizationStep(bool instationary,
                    const std::string& name,
                    const std::string& path,
                    const std::string& extendpath = "")
    : instationary(instationary)
    , time(0.0)
    , name(name)
    , path(path)
    , extendpath(extendpath)
  {}

  VisualizationStep(const Dune::ParameterTree& config)
    : instationary(config.get<bool>("instationary", false))
    , time(0.0)
    , name(config.get<std::string>("name", "output"))
    , path(config.get<std::string>("path", ""))
    , extendpath(config.get<std::string>("extendpath", ""))
  {}

  virtual ~VisualizationStep() {}

  virtual void update_parameter(std::string name, typename Traits::Parameter param) override
  {
    if (name == "time")
      time = std::get<double>(param);

    for (auto step : this->steps)
      step->update_parameter(name, param);
  }

  virtual void pre() override
  {
    // Instantiate a VTKWriter instance
    auto vector = this->solver->getVector();
    auto gv = vector->gridFunctionSpace().gridView();

    if (instationary)
      vtkwriter = std::make_shared<typename VTKWriterChooser<typename Traits::GridView, true>::type>(std::make_shared<typename VTKWriterChooser<typename Traits::GridView, false>::type>(gv), name, path, extendpath);
    else
      vtkwriter = std::make_shared<typename VTKWriterChooser<typename Traits::GridView, false>::type>(gv);

    for (auto step: this->steps)
    {
      auto vsp = dynamic_cast<VisualizationStepBase<V...>*>(step.get());
      if (vsp)
        vsp->set_parent(this);
      step->pre();
    }
  }

  virtual void apply() override
  {
    for (auto step: this->steps)
      step->apply();

    if (instationary)
    {
      std::filesystem::create_directory(std::filesystem::current_path().append(path));
      std::get<typename VTKWriterChooser<typename Traits::GridView, true>::ptype>(vtkwriter)->write(time, Dune::VTK::appendedraw);
    }
    else
      std::get<typename VTKWriterChooser<typename Traits::GridView, false>::ptype>(vtkwriter)->write(name, Dune::VTK::ascii);
  }

  template<typename Container>
  void add_dataset(std::shared_ptr<Container> container)
  {
    if (instationary)
      Dune::PDELab::addSolutionToVTKWriter(
          *(std::get<typename VTKWriterChooser<typename Traits::GridView, true>::ptype>(vtkwriter)),
          container->gridFunctionSpaceStorage(),
          container);
    else
      Dune::PDELab::addSolutionToVTKWriter(
          *(std::get<typename VTKWriterChooser<typename Traits::GridView, false>::ptype>(vtkwriter)),
          container->gridFunctionSpaceStorage(),
          container);
  };

  template<typename Container>
  void add_celldata(std::shared_ptr<Container> container, std::string name)
  {
    if (instationary)
      std::get<typename VTKWriterChooser<typename Traits::GridView, true>::ptype>(vtkwriter)->addCellData(*container, name);
    else
      std::get<typename VTKWriterChooser<typename Traits::GridView, false>::ptype>(vtkwriter)->addCellData(*container, name);
  }

  private:
  bool instationary;
  double time;
  std::string name;
  std::string path;
  std::string extendpath;
  VTKWriter vtkwriter;
};


template<std::size_t i, typename... V>
class SolutionVisualizationStep
  : public VisualizationStepBase<V...>
{
  public:
  using Traits = VectorStepTraits<i, V...>;

  virtual ~SolutionVisualizationStep() {}

  virtual void pre() override
  {
    auto vector = this->solver->template getVector<i>();
    this->parent->add_dataset(vector);
  }
};


template<typename... V>
class MPIRankVisualizationStep
  : public VisualizationStepBase<V...>
{
  public:
  using Traits = SimpleStepTraits<V...>;

  struct RankDummyContainer
  {
    RankDummyContainer(Dune::MPIHelper& helper, typename Traits::GridView gv) : rank(helper.rank()), size_(gv.size(0))
    {}

    double operator[](std::size_t i) const
    {
      return rank;
    }

    std::size_t size() const
    {
      return size_;
    }

    double rank;
    std::size_t size_;
  };

  MPIRankVisualizationStep(Dune::MPIHelper& helper)
    : helper(helper)
  {}

  virtual ~MPIRankVisualizationStep() {}

  virtual void pre() override
  {
    auto vector = this->solver->getVector();
    auto container = std::make_shared<RankDummyContainer>(helper, vector->gridFunctionSpace().gridView());
    this->parent->add_celldata(container, "mpirank");
  }

  private:
  Dune::MPIHelper& helper;
};


template<typename... V>
class PhysicalEntityVisualizationStep
  : public VisualizationStepBase<V...>
{
  public:
  using Traits = SimpleStepTraits<V...>;

  PhysicalEntityVisualizationStep(std::shared_ptr<std::vector<int>> physical)
    : physical(physical)
  {}

  virtual ~PhysicalEntityVisualizationStep() {}

  virtual void pre() override
  {
    this->parent->add_celldata(physical, "gmshPhysical");
  }

  private:
  std::shared_ptr<std::vector<int>> physical;
};


template<std::size_t i, typename... V>
class VonMisesStressVisualizationStep
  : public VisualizationStepBase<V...>
{
  public:
  using Traits = VectorStepTraits<i, V...>;

  VonMisesStressVisualizationStep(const Dune::ParameterTree& params)
    : continuous(params.get<bool>("continuous", false))
  {}

  virtual ~VonMisesStressVisualizationStep() {}

  virtual void update_parameter(std::string name, typename Traits::Parameter param) override
  {
    if (name == "material")
      material = std::get<std::shared_ptr<typename Traits::Material>>(param);
  }

  virtual void apply() override
  {
    auto vector = this->solver->template getVector<i>();
    auto es = vector->gridFunctionSpace().entitySet();
    auto gfs = vector->gridFunctionSpaceStorage();

    // A grid function for the stress
    VonMisesStressGridFunction<typename Traits::Vector, Traits::dim> stress(*vector, material);

    // Interpolate the stress into a grid function
    if (continuous)
    {
      using P1FEM = Dune::PDELab::PkLocalFiniteElementMap<typename Traits::EntitySet, double, typename Traits::Range, 1>;
      auto p1fem = std::make_shared<P1FEM>(es);
      using P1GFS = Dune::PDELab::GridFunctionSpace<typename Traits::EntitySet, P1FEM, Dune::PDELab::NoConstraints, typename Traits::VectorBackend>;
      auto p1gfs = std::make_shared<P1GFS>(es, p1fem);
      p1gfs->name("vonmises");
//     p0gfs->setDataSetType(Dune::PDELab::GridFunctionOutputParameters::Output::cellData);
      using StressVector = Dune::PDELab::Backend::Vector<P1GFS, typename Traits::ctype>;
      auto stress_container = std::make_shared<StressVector>(p1gfs);

      Dune::PDELab::interpolate(stress, *p1gfs, *stress_container);
      this->parent->add_dataset(stress_container);
    }
    else {
      using P0FEM = Dune::PDELab::P0LocalFiniteElementMap<typename Traits::ctype, typename Traits::Range, Traits::dim>;
      auto p0fem = std::make_shared<P0FEM>(Dune::GeometryTypes::simplex(Traits::dim));
      using P0GFS = Dune::PDELab::GridFunctionSpace<typename Traits::EntitySet, P0FEM, Dune::PDELab::NoConstraints, typename Traits::VectorBackend>;
      auto p0gfs = std::make_shared<P0GFS>(es, p0fem);
      p0gfs->name("vonmises");
      p0gfs->setDataSetType(Dune::PDELab::GridFunctionOutputParameters::Output::cellData);
      using StressVector = Dune::PDELab::Backend::Vector<P0GFS, typename Traits::ctype>;
      auto stress_container = std::make_shared<StressVector>(p0gfs);

      Dune::PDELab::interpolate(stress, *p0gfs, *stress_container);
      this->parent->add_dataset(stress_container);
    }
  }

  private:
  std::shared_ptr<typename Traits::Material> material;
  bool continuous;
};


template<typename... V>
class FibreDistanceVisualizationStep
  : public VisualizationStepBase<V...>
{
  public:
  using Traits = VectorStepTraits<0, V...>;

  FibreDistanceVisualizationStep(const Dune::ParameterTree& params, const Dune::ParameterTree& rootparams)
    : prestress(rootparams.sub(params.get<std::string>("key")), rootparams)
  {}

  virtual ~FibreDistanceVisualizationStep() {}

  virtual void pre() override
  {
    if constexpr (Traits::dim == 3)
    {
      auto vector = this->solver->getVector();
      auto es = vector->gridFunctionSpace().entitySet();
      using FEM = Dune::PDELab::PkLocalFiniteElementMap<typename Traits::EntitySet, double, typename Traits::Range, 1>;
      auto fem = std::make_shared<FEM>(es);
      using GFS = Dune::PDELab::GridFunctionSpace<typename Traits::EntitySet, FEM, Dune::PDELab::NoConstraints, typename Traits::VectorBackend>;
      auto gfs = std::make_shared<GFS>(es, fem);
      gfs->name("fibredistance");
      using DistanceVector = Dune::PDELab::Backend::Vector<GFS, typename Traits::ctype>;
      auto container = std::make_shared<DistanceVector>(gfs);

      auto lambda = [this](const auto& e, const auto& x){ return this->prestress.distance_to_minimum(e, x); };
      auto gf = Dune::PDELab::makeGridFunctionFromCallable(es.gridView(), lambda);
      Dune::PDELab::interpolate(gf, *gfs, *container);
      this->parent->add_dataset(container);
    }
  }

  private:
  CurvedFibrePrestress<typename Traits::GridView, double> prestress;
};

#endif
