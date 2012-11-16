// rbOOmit: An implementation of the Certified Reduced Basis method.
// Copyright (C) 2009, 2010 David J. Knezevic

// This file is part of rbOOmit.

// rbOOmit is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
  
// rbOOmit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __rb_system_h__
#define __rb_system_h__

#include "linear_implicit_system.h"
#include "dense_vector.h"
#include "dense_matrix.h"
#include "rb_base.h"
#include "fem_context.h"
#include "rb_evaluation.h"
#include "elem_assembly.h"

namespace libMesh
{

/**
 * This class is part of the rbOOmit framework.
 *
 * RBSystem implements the certified reduced
 * basis framework in the steady-state case.
 *
 * @author David J. Knezevic, 2009
 */


/**
 * First, define a class that allows us to assemble a list of Dirichlet
 * dofs. This list is used by RBSystem to impose homoegenous Dirichlet
 * boundary conditions (in the RB method, non-homogeneous Dirichlet
 * boundary conditions should be imposed via lifting functions).
 */
class DirichletDofAssembly : public ElemAssembly
{
public:
  std::set<unsigned int> dirichlet_dofs_set;
};

// ------------------------------------------------------------
// RBSystem class definition

class RBSystem : public RBBase<LinearImplicitSystem>
{
public:

  /**
   * Constructor.  Optionally initializes required
   * data structures.
   */
  RBSystem (EquationSystems& es,
            const std::string& name,
            const unsigned int number);

  /**
   * Destructor.
   */
  virtual ~RBSystem ();

  /**
   * The type of system.
   */
  typedef RBSystem sys_type;

  /**
   * @returns a clever pointer to the system.
   */
  sys_type & system () { return *this; }

  /**
   * The type of the parent.
   */
  typedef RBBase<LinearImplicitSystem> Parent;

  /**
   * Clear all the data structures associated with
   * the system.
   */
  virtual void clear ();

  /**
   * @returns a string indicating the type of the system.
   */
  virtual std::string system_type () const;

  /**
   * Perform a "truth" solve, i.e. solve the finite element system at
   * at the parameters currently set in the system. This is used
   * extensively in training the reduced basis, since "truth snapshots"
   * are employed as basis functions.
   */
  virtual Real truth_solve(int plot_solution);

  /**
   * Train the reduced basis. This is the crucial function in the Offline
   * stage: it generates the reduced basis using the "Greedy algorithm."
   * Each stage of the Greedy algorithm involves solving the reduced basis
   * over a large training set and selecting the parameter at which the
   * reduced basis error bound is largest, then performing a truth_solve
   * at that parameter and enriching the reduced basis with the corresponding
   * snapshot.
   *
   * @returns the final maximum a posteriori error bound on the training set.
   */
  virtual Real train_reduced_basis(const std::string& directory_name = "offline_data");

  /**
   * (i) Compute the a posteriori error bound for each set of parameters
   * in the training set, (ii) set current_parameters to the parameters that
   * maximize the error bound, and (iii) return the maximum error bound.
   */
  virtual Real compute_max_error_bound();

  /**
   * Return the parameters chosen during the i^th step of
   * the Greedy algorithm.
   */
  std::vector<Real> get_greedy_parameter(unsigned int i);

  /**
   * Set the name of the eigen_system that performs the SCM.
   */
  void set_eigen_system_name(const std::string& name);

  /**
   * Get/set the tolerance for the basis training.
   */
  void set_training_tolerance(Real training_tolerance)
    {this->training_tolerance = training_tolerance; }
  Real get_training_tolerance() { return training_tolerance; }

  /**
   * Get/set Nmax, the maximum number of RB
   * functions we are willing to compute.
   */
  unsigned int get_Nmax() const    { return Nmax; }
  virtual void set_Nmax(unsigned int Nmax);

  /**
   * Get Q_f, the number of terms in the affine
   * expansion for the right-hand side.
   */
  unsigned int get_Q_f() const { return theta_q_f_vector.size() + get_n_F_EIM_functions(); }

  /**
   * Get the number of EIM systems that will provide vectors on
   * the "right-hand side" of the PDE.
   */
  unsigned int get_n_F_EIM_systems() const;

  /**
   * Get the number of EIM RHS functions that are currently attached.
   */
  unsigned int get_n_F_EIM_functions() const;

  /**
   * Get n_outputs, the number output functionals.
   */
  unsigned int get_n_outputs() const { return theta_q_l_vector.size(); }
  
  /**
   * Get the number of affine terms associated with the specified output.
   */
  unsigned int get_Q_l(unsigned int output_index) const;

  /**
   * Set the quiet_mode flag. If quiet == false then
   * we print out a lot of extra information
   * during the Offline stage.
   */
  void set_quiet_mode(bool quiet_mode_in)
    { this->quiet_mode = quiet_mode_in; }

  /**
   * Is the system in quiet mode?
   */
  bool is_quiet() const
    { return this->quiet_mode; }

  /**
   * Load the i^th RB function into the RBSystem
   * solution vector.
   */
  virtual void load_basis_function(unsigned int i);

  /**
   * Get the current number of basis functions.
   */
  virtual unsigned int get_n_basis_functions() const { return rb_eval->get_n_basis_functions(); }

  /**
   * Get a reference to the i^th basis function stored in rb_eval.
   */
  NumericVector<Number>& get_basis_function(unsigned int i);

  /**
   * Load the RB solution from the most recent solve
   * into the libMesh solution vector.
   */
  virtual void load_RB_solution();

  /**
   * Register a user-defined object to be used in initializing the
   * lists of Dirichlet dofs.
   */
  void attach_dirichlet_dof_initialization (DirichletDofAssembly* dirichlet_init);

  /**
   * Call the user-defined Dirichlet dof initialization function.
   */
  void initialize_dirichlet_dofs();

  /**
   * Override attach_theta_q_a to just throw an error. Should
   * use attach_A_q in RBSystem and its subclasses.
   */
  virtual void attach_theta_q_a(RBTheta* )
  {
    libMesh::out << "Error: Cannot use attach_theta_q_a in RBSystem. Use attach_A_q instead." << std::endl;
    libmesh_error();
  }

  /**
   * Override attach_A_EIM_system to just throw an error. Should
   * use attach_A_EIM_system in RBSystem and its subclasses.
   */
  virtual void attach_A_EIM_system(RBEIMSystem*)
  {
    libMesh::out << "Error: Cannot use attach_theta_q_a in RBSystem. Use attach_A_q instead." << std::endl;
    libmesh_error();
  }

  /**
   * Attach parameter-dependent function and user-defined assembly routine
   * for affine operator (both interior and boundary assembly).
   */
  virtual void attach_A_q(RBTheta* theta_q_a,
                          ElemAssembly* A_q_assembly);

  /**
   * Attach parameter-dependent function and user-defined assembly routine
   * for affine vector. (Interior assembly and boundary assembly).
   */
  virtual void attach_F_q(RBTheta* theta_q_f,
                          ElemAssembly* F_q_assembly);

  /**
   * Attach an EIM system and a corresponding function pointers to
   * specify the assembly operation. Together these will provide
   * a set of LHS affine operators.
   */
  void attach_A_EIM_operators(RBEIMSystem* eim_system,
                              ElemAssembly* EIM_assembly);
  
  /**
   * Attach an EIM system and a corresponding function pointers to
   * specify the assembly operation. Together these will provide
   * a set of RHS affine functions.
   */
  void attach_F_EIM_vectors(RBEIMSystem* eim_system,
                            ElemAssembly* EIM_assembly);

  /**
   * @return a boolean to indicate whether the index q refers
   * to an RHS EIM operator.
   */
  bool is_F_EIM_function(unsigned int q);

  /**
   * Attach user-defined assembly routine
   * for the inner-product matrix.
   */
  void attach_inner_prod_assembly(ElemAssembly* IP_assembly);

  /**
   * Attach user-defined assembly routine
   * for the constraint matrix.
   */
  void attach_constraint_assembly(ElemAssembly* constraint_assembly_in);

  /**
   * Attach user-defined assembly routine for output. 
   * (Interior assembly and boundary assembly).
   * In this case we pass in vector arguments to allow for Q_l > 1.
   */
  virtual void attach_output(std::vector<RBTheta*> theta_q_l,
                             std::vector<ElemAssembly*> output_assembly);

  /**
   * Attach user-defined assembly routine for output. 
   * (Interior assembly and boundary assembly).
   * This function provides simpler syntax in the case that Q_l = 1; we
   * do not need to use a vector in this case.
   */
  virtual void attach_output(RBTheta* theta_q_l,
                             ElemAssembly* output_assembly);

  /**
   * Get a pointer to inner_product_matrix. Accessing via this
   * function, rather than directly through the class member allows
   * us to do error checking (e.g. inner_product_matrix is not
   * defined in low-memory mode).
   */
  SparseMatrix<Number>* get_inner_product_matrix();

  /**
   * Get a pointer to non_dirichlet_inner_product_matrix.
   * Accessing via this function, rather than directly through
   * the class member allows us to do error checking (e.g.
   * non_dirichlet_inner_product_matrix is not
   * defined in low-memory mode, and we need
   * store_non_dirichlet_operators==true).
   */
  SparseMatrix<Number>* get_non_dirichlet_inner_product_matrix();

  /**
   * Get a pointer to A_q.
   */
  SparseMatrix<Number>* get_A_q(unsigned int q);

  /**
   * Get a pointer to non_dirichlet_A_q.
   */
  SparseMatrix<Number>* get_non_dirichlet_A_q(unsigned int q);
  
  /**
   * Get a reference to the specified LHS EIM system.
   */
  RBEIMSystem& get_A_EIM_system(unsigned int index);
  
  /**
   * Get a reference to the specified RHS EIM system.
   */
  RBEIMSystem& get_F_EIM_system(unsigned int index);
  
  /**
   * Evaluate the EIM function that is currently being employed
   * for affine assembly.
   * @return the function values at the \p qpoints, where qpoints
   * are on \p element.
   */
  std::vector<Number> evaluate_current_EIM_function(Elem& element,
                                                    const std::vector<Point>& qpoints);

  /**
   * Evaluate theta_q_f at the current parameter.
   */
  virtual Number eval_theta_q_f(unsigned int q);

  /**
   * Evaluate theta_q_l at the current parameter.
   */
  Number eval_theta_q_l(unsigned int output_index, unsigned int q_l);

  /**
   * Resize all the RB matrices.
   * Optionally assemble and store the Dirichlet dof lists, the
   * affine and output vectors.
   * Optionally assemble and store all the affine matrices if we
   * are not in low-memory mode.
   * The boolean argument \p do_not_assemble is true, then
   * we do not perform the initial assembly of affine operators
   * and vectors.
   */
  virtual void initialize_RB_system(bool do_not_assemble);

  /**
   * Get a pointer to F_q.
   */
  NumericVector<Number>* get_F_q(unsigned int q);

  /**
   * Get a pointer to non-Dirichlet F_q.
   */
  NumericVector<Number>* get_non_dirichlet_F_q(unsigned int q);

  /**
   * Get a pointer to the n^th output.
   */
  NumericVector<Number>* get_output_vector(unsigned int n, unsigned int q_l);

  /**
   * Assemble the inner product matrix and store it in input_matrix.
   */
  void assemble_inner_product_matrix(SparseMatrix<Number>* input_matrix, bool apply_dirichlet_bc=true);

  /**
   * Assemble the constraint matrix and store it in input_matrix.
   */
  void assemble_constraint_matrix(SparseMatrix<Number>* input_matrix);

  /**
   * Assemble the constraint matrix and add it to input_matrix.
   */
  void assemble_and_add_constraint_matrix(SparseMatrix<Number>* input_matrix);

  /**
   * Assemble the q^th affine matrix and store it in input_matrix.
   */
  void assemble_Aq_matrix(unsigned int q, SparseMatrix<Number>* input_matrix, bool apply_dirichlet_bc=true);

  /**
   * Assemble the q^th affine vector and store it in input_matrix.
   */
  void assemble_Fq_vector(unsigned int q, NumericVector<Number>* input_vector, bool apply_dirichlet_bc=true);

  /**
   * Add the scaled q^th affine matrix to input_matrix. If symmetrize==true, then
   * we symmetrize Aq before adding it.
   */
  void add_scaled_Aq(Number scalar, unsigned int q_a,
                     SparseMatrix<Number>* input_matrix,
                     bool symmetrize);
  
  /**
   * A convenient enum that allows us to specify whether we want
   * to write out all the data, or just a basis (in)dependent subset.
   */
  enum RBDataIO { ALL_DATA          = 1,
                  BASIS_DEPENDENT   = 2,
                  BASIS_INDEPENDENT = 3 };

  /**
   * Write out all the data to text files in order to segregate the
   * Offline stage from the Online stage.
   * \p directory_name specifies which directory to write to.
   * \p io_flag specifies whether we write out all data, or only
   * a basis (in)dependent subset.
   */
  virtual void write_offline_data_to_files(const std::string& directory_name = "offline_data",
                                           const RBDataIO io_flag = ALL_DATA);

  /**
   * Read in the saved Offline reduced basis data
   * to initialize the system for Online solves.
   * \p directory_name specifies which directory to read from.
   * \p io_flag specifies whether we read in all data, or only
   * a basis (in)dependent subset.
   */
  virtual void read_offline_data_from_files(const std::string& directory_name = "offline_data",
                                            const RBDataIO io_flag = ALL_DATA);
  
  /**
   * This function computes all of the residual representors, can be useful
   * when restarting a basis training computation.
   * If \p compute_inner_products is false, we just compute the residual Riesz
   * representors, whereas if true, we also compute all the corresponding inner
   * product terms.
   */
  virtual void recompute_all_residual_terms(const bool compute_inner_products=true);

  /**
   * Build a new RBEvaluation object and add
   * it to the rb_evaluation_objects vector.
   * @return a pointer to the new RBEvaluation object.
   */
  virtual RBEvaluation* add_new_rb_evaluation_object();
  
  /**
   * Evaluate the dual norm of output \p n
   * for the current parameters.
   */
  Real eval_output_dual_norm(unsigned int n);

  /**
   * Specifies the residual scaling on the denominator to
   * be used in the a posteriori error bound. Overload
   * in subclass in order to obtain the desired error bound.
   */
  virtual Real residual_scaling_denom(Real alpha_LB);

  /**
   * Get delta_N, the number of basis functions we
   * add to the RB space per iteration of the greedy
   * algorithm. For steady-state systems, this should
   * be 1, but can be more than 1 for time-dependent
   * systems.
   */
  unsigned int get_delta_N() const { return delta_N; }

  /**
   * Get the SCM lower bound at the current parameter value.
   */
  virtual Real get_SCM_lower_bound();

  /**
   * Get the SCM upper bound at the current parameter value.
   */
  virtual Real get_SCM_upper_bound();


  //----------- PUBLIC DATA MEMBERS -----------//

  /**
   * The current RBEvaluation object we are using.
   */
  RBEvaluation* rb_eval;

  /**
   * The set of RBEvaluation objects. These perform all
   * the "online" RB evaluation calculations, and store
   * the associated data required for those calculations.
   *
   * Often we will just have one RBEvaluation object
   * associated with an RBSystem, but it can be useful
   * to be able to "switch contexts" between different
   * RBEvaluations, e.g. in the context of the hp-RB method.
   */
  std::vector<RBEvaluation*> rb_evaluation_objects;

  /**
   * Vector storing the values of the error bound
   * for each parameter in the training set --- the
   * parameter giving the largest error bound is
   * chosen for the next snapshot in the Greedy basis
   * training.
   */
  std::vector<Real> training_error_bounds;

  /**
   * The inner product matrix.
   */
  AutoPtr< SparseMatrix<Number> > inner_product_matrix;

  /**
   * The inner product matrix without Dirichlet conditions enforced.
   * (This is only computed if store_non_dirichlet_operators == true.)
   */
  AutoPtr< SparseMatrix<Number> > non_dirichlet_inner_product_matrix;

  /**
   * The constraint matrix, e.g. the pressure matrix entries
   * in a Stokes problem.
   */
  AutoPtr< SparseMatrix<Number> > constraint_matrix;

  /**
   * Vector storing the truth output values from the most
   * recent truth solve.
   */
  std::vector< Number > truth_outputs;

  /**
   * The vector storing the dual norm inner product terms
   * for each output.
   */
  std::vector< std::vector< Number > > output_dual_norms;

  /**
   * Vector storing the residual representors associated with the
   * right-hand side.
   * These are basis independent and hence stored here, whereas
   * the A_q_representors are stored in RBEvaluation
   */
  std::vector< NumericVector<Number>* > F_q_representor;

  /**
   * Vectors storing the residual representor inner products
   * to be used in computing the residuals online. We store
   * the Fq representor norms here because they are independent
   * of a reduced basis space. The basis dependent representors
   * are stored in RBEvaluation.
   */
  std::vector<Number> Fq_representor_norms;

  /**
   * Set storing the global Dirichlet dof indices.
   * We update this set in initialize_dirichlet_dofs().
   */
  std::set<unsigned int> global_dirichlet_dofs_set;

  /**
   * Boolean flag to indicate whether this is a constrained problem
   * (e.g. Stokes) or not.
   */
  bool constrained_problem;

  /**
   * Boolean flag to indicate whether the basis functions are written
   * out from the Offline stage or read in during the Online stage.
   */
  bool store_basis_functions;

  /**
   * Boolean flag to indicate whether the residual representors are
   * written out from the offline stage.  For large problems, reading
   * them back in during a restart can be much faster than recomputing
   * them.
   */
  bool store_representors;

  /**
   * Boolean flag to indicate whether or not we are in "low-memory" mode.
   * In low-memory mode, we do not store any extra sparse matrices.
   */
  bool low_memory_mode;

  /**
   * Boolean flag to indicate whether we reuse the preconditioner
   * on consecutive Offline solves for updating residual data.
   */
  bool reuse_preconditioner;

  /**
   * Boolean flag to indicate whether RB_solve returns an absolute
   * or relative error bound. True => relative, false => absolute.
   */
  bool return_rel_error_bound;

  /**
   * Boolean flag to indicate whether train_reduced_basis writes
   * out offline data after each truth solve (to allow continuing
   * in case the code crashes or something).
   */
  bool write_data_during_training;

  /**
   * Boolean flag to indicate whether we require a Dirichlet boundary
   * condition on internal mesh dofs, for example in a problem in which
   * parameters are not defined on some subdomains.
   */
  bool impose_internal_dirichlet_BCs;

  /**
   * Boolean flag to indicate whether we impose flux on internal element
   * boundaries.
   */
  bool impose_internal_fluxes;
  
  /**
   * Boolean flag to indicate whether we compute the RB_inner_product_matrix
   */
  bool compute_RB_inner_product;

  /**
   * Boolean flag to indicate whether we store a second copy of each
   * affine operator and vector which does not have Dirichlet bcs
   * enforced.
   */
  bool store_non_dirichlet_operators;

  /**
   * The filename of the text file from which we read in the
   * problem parameters. We use getpot.h to perform the reading.
   */
  std::string parameters_filename;
  
  /**
   * Public member variable which we use to determine whether or
   * not we enforce hanging-dof and/or periodic constraints exactly.
   * This is primarily important in nonlinear problems where we may
   * "undersolve" Newton iterates for the sake of efficiency.
   */
  bool enforce_constraints_exactly;

  /**
   * Controls whether or not XDR (binary) files are written out for
   * the basis functions.  The binary file size can be as small
   * as 1/3 the size of an ASCII file.
   */
  bool write_binary_basis_functions;

  /**
   * Controls wether XDR (binary) files are read for
   * the basis functions.  Note: if you wrote ASCII basis functions
   * during a previous run but want to start writing XDR, set
   * read_binary_basis_functions = false and
   * write_binary_basis_functions = true.
   */
  bool read_binary_basis_functions;

  /**
   * Controls whether or not XDR (binary) files are written out for
   * the residual respresentors.  The binary file size can be as small
   * as 1/3 the size of an ASCII file.
   */
  bool write_binary_residual_representors;

  /**
   * Controls wether XDR (binary) files are read for
   * the residual representors.  Note: if you wrote ASCII representors
   * during a previous run but want to start writing XDR, set
   * read_binary_residual_representors = false and
   * write_binary_residual_representors = true.
   */
  bool read_binary_residual_representors;

  /**
   * A boolean flag to indicate whether or not we initialize the
   * Greedy algorithm by performing RB_solves on the training set
   * with an "empty" (i.e. N=0) reduced basis space.
   */
  bool use_empty_RB_solve_in_greedy;

protected:

  /**
   * Read in the parameters from file and set up the system
   * accordingly.
   */
  virtual void process_parameters_file();
  
  /**
   * Helper function that actually allocates all the data
   * structures required by this class.
   */
  virtual void allocate_data_structures();

  /**
   * Assemble the truth matrix and right-hand side
   * for current_parameters.
   */
  virtual void truth_assembly();

  /**
   * Builds a FEMContext object with enough information to do
   * evaluations on each element.
   */
  virtual AutoPtr<FEMContext> build_context();
  
  /**
   * Define the matrix assembly for the output residual dual
   * norm solves. By default we use the inner product matrix
   * for steady state problems.
   */
  virtual void assemble_matrix_for_output_dual_solves();

  /**
   * Function that indicates when to terminate the Greedy
   * basis training. Overload in subclasses to specialize.
   */
  virtual bool greedy_termination_test(Real training_greedy_error, int count);

  /**
   * Update the list of Greedily chosen parameters with
   * current_parameters.
   */
  void update_greedy_param_list();

  /**
   * This function loops over the mesh and applies the specified
   * interior and/or boundary assembly routines, then adds the
   * scaled result to input_matrix and/or input_vector.
   * If symmetrize==true then we assemble the symmetric part
   * of the matrix, 0.5*(A + A^T)
   */
  void add_scaled_matrix_and_vector(Number scalar,
                                    ElemAssembly* elem_assembly,
                                    SparseMatrix<Number>* input_matrix,
                                    NumericVector<Number>* input_vector,
                                    bool symmetrize=false,
                                    bool apply_dirichlet_bc=true);

  /**
   * Set current_local_solution = vec so that we can access vec
   * from FEMContext during assembly. Overload in subclasses if
   * different behavior is required (e.g. in QNTransientRBSystem)
   */
  virtual void set_context_solution_vec(NumericVector<Number>& vec);

  /**
   * This function loops over the mesh and assembles the
   * matrix-vector product and stores the scaled result
   * in dest.
   */
  void assemble_scaled_matvec(Number scalar,
                              ElemAssembly* elem_assembly,
                              NumericVector<Number>& dest,
                              NumericVector<Number>& arg);

  /**
   * Assemble and store all the inner-product
   * matrix, the constraint matrix (for constrained
   * problems) and the mass matrix (for time-dependent
   * problems).
   */
  virtual void assemble_misc_matrices();

  /**
   * Assemble and store all Q_a affine operators
   * as well as the inner-product matrix.
   */
  virtual void assemble_all_affine_operators();

  /**
   * Assemble and store the affine RHS vectors.
   */
  virtual void assemble_all_affine_vectors();

  /**
   * Assemble and store the output vectors.
   */
  virtual void assemble_all_output_vectors();

  /**
   * Compute and store the dual norm of each output functional.
   */
  virtual void compute_output_dual_norms();

  /**
   * Compute the terms that are combined `online'
   * to determine the dual norm of the residual. Here we
   * compute the terms associated with the right-hand side.
   * These terms are basis independent, hence we separate
   * them from the rest of the calculations that are done in
   * update_residual_terms.
   * By default,
   * inner product terms are also computed, but you can turn
   * this feature off e.g. if you are already reading in that
   * data from files.
   */
  virtual void compute_Fq_representor_norms(bool compute_inner_products=true);

  /**
   * Add a new basis function to the RB space. This is called
   * during train_reduced_basis.
   */
  virtual void enrich_RB_space();

  /**
   * Update the system after enriching the RB space; this calls
   * a series of functions to update the system properly.
   */
  virtual void update_system();
  
  /**
   * This function returns the RB error bound for the current parameters and
   * is used in the Greedy algorithm to select the next parameter.
   */
  virtual Real get_RB_error_bound() { return rb_eval->RB_solve(get_n_basis_functions()); }

  /**
   * Compute the reduced basis matrices for the current basis.
   */
  virtual void update_RB_system_matrices();

  /**
   * Compute the terms that are combined `online'
   * to determine the dual norm of the residual.  By default,
   * inner product terms are also computed, but you can turn
   * this feature off e.g. if you are already reading in that
   * data from files.
   */
  virtual void update_residual_terms(bool compute_inner_products=true);

  /**
   * Initialize the FEMContext prior to performing
   * an element loop.
   * Reimplement this in derived classes in order to
   * call FE::get_*() as the particular physics requires.
   */
  virtual void init_context(FEMContext& ) {}

  /**
   * Set the dofs on the Dirichlet boundary (stored in
   * the set global_dirichlet_dofs) to zero
   * in the right-hand side vector.
   */
  void zero_dirichlet_dofs_on_rhs();

  /**
   * Set the dofs on the Dirichlet boundary (stored in
   * the set global_dirichlet_dofs) to zero
   * in the vector temp.
   */
  void zero_dirichlet_dofs_on_vector(NumericVector<Number>& temp);
  
  /**
   * @return the EIM system and affine function indices associated with
   * the RHS index q.
   */
  std::pair<unsigned int, unsigned int> get_F_EIM_indices(unsigned int q);


  //----------- PROTECTED DATA MEMBERS -----------//

  /**
   * Maximum number of reduced basis
   * functions we are willing to use.
   */
  unsigned int Nmax;

 /**
   * The number of basis functions that we add at each greedy step.
   * This defaults to 1 in the steady case.
   */
  unsigned int delta_N;

  /**
   * Flag to indicate whether we print out extra information during
   * the Offline stage.
   */
  bool quiet_mode;

  /**
   * The name of the RBSCMSystem system that performs
   * the SCM.
   */
  std::string eigen_system_name;

  /**
   * Pointer to inner product assembly.
   */
  ElemAssembly* inner_prod_assembly;

  /**
   * Function pointer for assembling the constraint
   * matrix.
   */
  ElemAssembly* constraint_assembly;

  /**
   * Vectors storing the function pointers to the assembly
   * routines for the affine operators, both interior and boundary
   * assembly.
   */
  std::vector<ElemAssembly*> A_q_assembly_vector;

  /**
   * Vector storing the function pointers to the assembly
   * routines for the rhs affine vectors.
   */
  std::vector<ElemAssembly*> F_q_assembly_vector;

  /**
   * Vector storing the function pointers to the assembly
   * routines for the outputs. Element interior part.
   */
  std::vector< std::vector<ElemAssembly*> > output_assembly_vector;

  /**
   * A boolean flag to indicate whether or not the output dual norms
   * have already been computed --- used to make sure that we don't
   * recompute them unnecessarily.
   */
  bool output_dual_norms_computed;

  /**
   * A boolean flag to indicate whether or not the Fq representor norms
   * have already been computed --- used to make sure that we don't
   * recompute them unnecessarily.
   */
  bool Fq_representor_norms_computed;

private:

  //----------- PRIVATE DATA MEMBERS -----------//

  /**
   * Vector storing the RBTheta functors for the theta_q_f (affine expansion of the rhs).
   */
  std::vector<RBTheta*> theta_q_f_vector;

  /**
   * Vector storing the RBTheta functors for the theta_q_l (affine expansion of the outputs).
   */
  std::vector< std::vector<RBTheta*> > theta_q_l_vector;
  
  /**
   * Vector storing the EIM systems that provide additional affine functions
   * on the "right-hand side" of the PDE.
   */
  std::vector< RBEIMSystem* > F_EIM_systems_vector;

  /**
   * Vector storing the function pointers to the assembly
   * routines for the LHS EIM affine operators.
   */
  std::vector<ElemAssembly*> A_EIM_assembly_vector;

  /**
   * Vector storing the function pointers to the assembly
   * routines for the RHS EIM affine operators.
   */
  std::vector<ElemAssembly*> F_EIM_assembly_vector;

  /**
   * Vector storing the Q_a matrices from the affine expansion
   */
  std::vector< SparseMatrix<Number>* > A_q_vector;

  /**
   * Vector storing the Q_f vectors in the affine decomposition
   * of the right-hand side.
   */
  std::vector< NumericVector<Number>* > F_q_vector;

  /**
   * We sometimes also need a second set of matrices/vectors
   * that do not have the Dirichlet boundary conditions
   * enforced.
   */
  std::vector< SparseMatrix<Number>* > non_dirichlet_A_q_vector;
  std::vector< NumericVector<Number>* > non_dirichlet_F_q_vector;

  /**
   * The libMesh vectors that define the output functionals.
   * Each row corresponds to the affine expansion of an output.
   */
  std::vector< std::vector< NumericVector<Number>* > > outputs_vector;

  /**
   * Tolerance for training reduced basis using the Greedy scheme.
   */
  Real training_tolerance;

  /**
   * Assembly object that is used to initialize the list of Dirichlet dofs.
   */
  DirichletDofAssembly* _dirichlet_list_init;

  /**
   * Boolean flag to indicate whether the RBSystem has been initialized.
   */
  bool RB_system_initialized;
  
  /**
   * Pointer to the current LHS or RHS EIM systems. This pointer
   * is used during assembly routines in order to loop over the
   * set of LHS and RHS EIM functions. The user can access the results
   * of EIM function evaluation via evaluate_current_EIM_function.
   */
  RBEIMSystem* current_EIM_system;
};

} // namespace libMesh

#endif