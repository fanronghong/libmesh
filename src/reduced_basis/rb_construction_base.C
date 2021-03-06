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

// C++ includes
#include <ctime>
#include <cstdlib> // *must* precede <cmath> for proper std:abs() on PGI, Sun Studio CC
#include <cmath>

// rbOOmit includes
#include "libmesh/rb_construction_base.h"

// libMesh includes
#include "libmesh/libmesh_logging.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/equation_systems.h"
#include "libmesh/parallel.h"
#include "libmesh/petsc_linear_solver.h"
#include "libmesh/condensed_eigen_system.h"
#include "libmesh/linear_implicit_system.h"
#include "libmesh/int_range.h"

namespace libMesh
{

// ------------------------------------------------------------
// RBConstructionBase implementation


template <class Base>
RBConstructionBase<Base>::RBConstructionBase (EquationSystems & es,
                                              const std::string & name_in,
                                              const unsigned int number_in)
  : Base(es, name_in, number_in),
    quiet_mode(true),
    serial_training_set(false),
    training_parameters_initialized(false),
    training_parameters_random_seed(-1) // by default, use std::time to seed RNG
{
  training_parameters.clear();
}

template <class Base>
RBConstructionBase<Base>::~RBConstructionBase ()
{
  this->clear();
}

template <class Base>
void RBConstructionBase<Base>::clear ()
{
  // clear the parent data
  Base::clear();
  RBParametrized::clear();
  training_parameters.clear();
}

template <class Base>
void RBConstructionBase<Base>::init_data ()
{
  Base::init_data();

  // Initialize the inner product storage vector, which is useful for
  // storing intermediate results when evaluating inner products
  inner_product_storage_vector = NumericVector<Number>::build(this->comm());
  inner_product_storage_vector->init (this->n_dofs(), this->n_local_dofs(), false, PARALLEL);
}

template <class Base>
void RBConstructionBase<Base>::get_global_max_error_pair(const Parallel::Communicator & communicator,
                                                         std::pair<numeric_index_type, Real> & error_pair)
{
  // Set error_pair.second to the maximum global value and also
  // find which processor contains the maximum value
  unsigned int proc_ID_index;
  communicator.maxloc(error_pair.second, proc_ID_index);

  // Then broadcast error_pair.first from proc_ID_index
  communicator.broadcast(error_pair.first, proc_ID_index);
}

template <class Base>
numeric_index_type RBConstructionBase<Base>::get_n_training_samples() const
{
  libmesh_assert(training_parameters_initialized);

  if (training_parameters.empty())
    return 0;

  return training_parameters.begin()->second->size();
}

template <class Base>
numeric_index_type RBConstructionBase<Base>::get_local_n_training_samples() const
{
  libmesh_assert(training_parameters_initialized);
  return training_parameters.begin()->second->local_size();
}

template <class Base>
numeric_index_type RBConstructionBase<Base>::get_first_local_training_index() const
{
  libmesh_assert(training_parameters_initialized);
  return training_parameters.begin()->second->first_local_index();
}

template <class Base>
numeric_index_type RBConstructionBase<Base>::get_last_local_training_index() const
{
  libmesh_assert(training_parameters_initialized);
  return training_parameters.begin()->second->last_local_index();
}

template <class Base>
void RBConstructionBase<Base>::set_params_from_training_set(unsigned int index)
{
  set_parameters(get_params_from_training_set(index));
}

template <class Base>
RBParameters RBConstructionBase<Base>::get_params_from_training_set(unsigned int index)
{
  libmesh_assert(training_parameters_initialized);

  libmesh_assert( (this->get_first_local_training_index() <= index) &&
                  (index < this->get_last_local_training_index()) );

  RBParameters params;
  for (const auto & pr : training_parameters)
    {
      const std::string & param_name = pr.first;
      Real param_value = libmesh_real((*(pr.second))(index));
      params.set_value(param_name, param_value);
    }

  return params;
}

template <class Base>
void RBConstructionBase<Base>::set_params_from_training_set_and_broadcast(unsigned int index)
{
  libmesh_assert(training_parameters_initialized);

  processor_id_type root_id = 0;
  if ((this->get_first_local_training_index() <= index) &&
      (index < this->get_last_local_training_index()))
    {
      // Set parameters on only one processor
      set_params_from_training_set(index);

      // set root_id, only non-zero on one processor
      root_id = this->processor_id();
    }

  // broadcast
  this->comm().max(root_id);
  broadcast_parameters(root_id);
}

template <class Base>
void RBConstructionBase<Base>::initialize_training_parameters(const RBParameters & mu_min,
                                                              const RBParameters & mu_max,
                                                              unsigned int n_training_samples,
                                                              std::map<std::string,bool> log_param_scale,
                                                              bool deterministic)
{
  if(!is_quiet())
    {
      // Print out some info about the training set initialization
      libMesh::out << "Initializing training parameters with "
                  << (deterministic ? "deterministic " : "random " )
                  << "training set..." << std::endl;

      for (const auto & pr : log_param_scale)
        libMesh::out << "Parameter "
                     << pr.first
                     << ": log scaling = "
                     << pr.second
                     << std::endl;

      libMesh::out << std::endl;
    }

  if (deterministic)
    {
      generate_training_parameters_deterministic(this->comm(),
                                                 log_param_scale,
                                                 training_parameters,
                                                 n_training_samples,
                                                 mu_min,
                                                 mu_max,
                                                 serial_training_set);
    }
  else
    {
      // Generate random training samples for all parameters
      generate_training_parameters_random(this->comm(),
                                          log_param_scale,
                                          training_parameters,
                                          n_training_samples,
                                          mu_min,
                                          mu_max,
                                          this->training_parameters_random_seed,
                                          serial_training_set);
    }

  // For each parameter that only allows discrete values, we "snap" to the nearest
  // allowable discrete value
  if (get_n_discrete_params() > 0)
    {
      for (const auto & pr : training_parameters)
        {
          const std::string & param_name = pr.first;
          if (is_discrete_parameter(param_name))
            {
              std::vector<Real> discrete_values =
                get_discrete_parameter_values().find(param_name)->second;

              NumericVector<Number> * training_vector = pr.second.get();

              for (numeric_index_type index=training_vector->first_local_index();
                   index<training_vector->last_local_index();
                   index++)
                {
                  Real value = libmesh_real((*training_vector)(index));
                  Real nearest_discrete_value = get_closest_value(value, discrete_values);
                  training_vector->set(index, nearest_discrete_value);
                }
            }
        }
    }

  training_parameters_initialized = true;
}

template <class Base>
void RBConstructionBase<Base>::load_training_set(std::map<std::string, std::vector<Number>> & new_training_set)
{
  // First, make sure that an initial training set has already been
  // generated
  if (!training_parameters_initialized)
    libmesh_error_msg("Error: load_training_set cannot be used to initialize parameters");

  // Make sure that the training set has the correct number of parameters
  if (new_training_set.size() != get_n_params())
    libmesh_error_msg("Error: Incorrect number of parameters in load_training_set.");

  // Delete the training set vectors (but don't remove the existing keys!)
  for (auto & pr : training_parameters)
    pr.second.reset(nullptr);

  // Get the number of local and global training parameters
  numeric_index_type n_local_training_samples  =
    cast_int<numeric_index_type>(new_training_set.begin()->second.size());
  numeric_index_type n_global_training_samples = n_local_training_samples;
  this->comm().sum(n_global_training_samples);

  for (auto & pr : training_parameters)
    {
      pr.second = NumericVector<Number>::build(this->comm());
      pr.second->init(n_global_training_samples, n_local_training_samples, false, PARALLEL);
    }

  for (auto & pr : training_parameters)
    {
      const std::string & param_name = pr.first;
      NumericVector<Number> * training_vector = pr.second.get();

      numeric_index_type first_index = training_vector->first_local_index();
      for (numeric_index_type i=0; i<n_local_training_samples; i++)
        {
          numeric_index_type index = first_index + i;
          training_vector->set(index, new_training_set[param_name][i]);
        }
    }
}


template <class Base>
void RBConstructionBase<Base>::generate_training_parameters_random(const Parallel::Communicator & communicator,
                                                                   std::map<std::string, bool> log_param_scale,
                                                                   std::map<std::string, std::unique_ptr<NumericVector<Number>>> & training_parameters_in,
                                                                   unsigned int n_training_samples_in,
                                                                   const RBParameters & min_parameters,
                                                                   const RBParameters & max_parameters,
                                                                   int training_parameters_random_seed,
                                                                   bool serial_training_set)
{
  libmesh_assert_equal_to ( min_parameters.n_parameters(), max_parameters.n_parameters() );
  const unsigned int num_params = min_parameters.n_parameters();

  // Clear training_parameters_in
  training_parameters_in.clear();

  if (num_params == 0)
    return;

  if (training_parameters_random_seed < 0)
    {
      if (!serial_training_set)
        {
          // seed the random number generator with the system time
          // and the processor ID so that the seed is different
          // on different processors
          std::srand( static_cast<unsigned>( std::time(0)*(1+communicator.rank()) ));
        }
      else
        {
          // seed the random number generator with the system time
          // only so that the seed is the same on all processors
          //
          // Note that we broadcast the time on processor 0 to make
          // sure all processors agree.
          unsigned int current_time = static_cast<unsigned>( std::time(0) );
          communicator.broadcast(current_time, 0);
          std::srand(current_time);
        }
    }
  else
    {
      if (!serial_training_set)
        {
          // seed the random number generator with the provided value
          // and the processor ID so that the seed is different
          // on different processors
          std::srand( static_cast<unsigned>( training_parameters_random_seed*(1+communicator.rank()) ));
        }
      else
        {
          // seed the random number generator with the provided value
          // so that the seed is the same on all processors
          std::srand( static_cast<unsigned>( training_parameters_random_seed ));
        }
    }

  // initialize training_parameters_in
    for (const auto & pr : min_parameters)
      {
        const std::string & param_name = pr.first;
        training_parameters_in[param_name] = NumericVector<Number>::build(communicator);

        if (!serial_training_set)
          {
            // Calculate the number of training parameters local to this processor
            unsigned int n_local_training_samples;
            unsigned int quotient  = n_training_samples_in/communicator.size();
            unsigned int remainder = n_training_samples_in%communicator.size();
            if (communicator.rank() < remainder)
              n_local_training_samples = (quotient + 1);
            else
              n_local_training_samples = quotient;

            training_parameters_in[param_name]->init(n_training_samples_in, n_local_training_samples, false, PARALLEL);
          }
        else
          {
            training_parameters_in[param_name]->init(n_training_samples_in, false, SERIAL);
          }
      }

  // finally, set the values
  for (auto & pr : training_parameters_in)
    {
      const std::string & param_name = pr.first;
      NumericVector<Number> * training_vector = pr.second.get();

      numeric_index_type first_index = training_vector->first_local_index();
      for (auto i : IntRange<numeric_index_type>(0, training_vector->local_size()))
        {
          numeric_index_type index = first_index + i;
          Real random_number = static_cast<Real>(std::rand()) / RAND_MAX; // in range [0,1]

          // Generate log10 scaled training parameters
          if (log_param_scale[param_name])
            {
              Real log_min   = log10(min_parameters.get_value(param_name));
              Real log_range = log10(max_parameters.get_value(param_name) / min_parameters.get_value(param_name));

              training_vector->set(index, pow(10., log_min + random_number*log_range ) );
            }
          // Generate linearly scaled training parameters
          else
            {
              training_vector->set(index, random_number*(max_parameters.get_value(param_name) - min_parameters.get_value(param_name))
                                   + min_parameters.get_value(param_name));
            }
        }
    }
}

template <class Base>
void RBConstructionBase<Base>::generate_training_parameters_deterministic(const Parallel::Communicator & communicator,
                                                                          std::map<std::string, bool> log_param_scale,
                                                                          std::map<std::string, std::unique_ptr<NumericVector<Number>>> & training_parameters_in,
                                                                          unsigned int n_training_samples_in,
                                                                          const RBParameters & min_parameters,
                                                                          const RBParameters & max_parameters,
                                                                          bool serial_training_set)
{
  libmesh_assert_equal_to ( min_parameters.n_parameters(), max_parameters.n_parameters() );
  const unsigned int num_params = min_parameters.n_parameters();

  if (num_params == 0)
    return;

  if (num_params > 2)
    {
      libMesh::out << "ERROR: Deterministic training sample generation "
                   << " not implemented for more than two parameters." << std::endl;
      libmesh_not_implemented();
    }

  // Clear training_parameters_in (but don't remove existing keys!)
  for (auto & pr : training_parameters_in)
    pr.second.reset(nullptr);

  // Initialize training_parameters_in
    for (const auto & pr : min_parameters)
      {
        const std::string & param_name = pr.first;
        training_parameters_in[param_name] = NumericVector<Number>::build(communicator);

        if (!serial_training_set)
          {
            // Calculate the number of training parameters local to this processor
            unsigned int n_local_training_samples;
            unsigned int quotient  = n_training_samples_in/communicator.size();
            unsigned int remainder = n_training_samples_in%communicator.size();
            if (communicator.rank() < remainder)
              n_local_training_samples = (quotient + 1);
            else
              n_local_training_samples = quotient;

            training_parameters_in[param_name]->init(n_training_samples_in, n_local_training_samples, false, PARALLEL);
          }
        else
          {
            training_parameters_in[param_name]->init(n_training_samples_in, false, SERIAL);
          }
      }

  if (num_params == 1)
    {
      NumericVector<Number> * training_vector = training_parameters_in.begin()->second.get();
      bool use_log_scaling = log_param_scale.begin()->second;
      Real min_param = min_parameters.begin()->second;
      Real max_param = max_parameters.begin()->second;

      numeric_index_type first_index = training_vector->first_local_index();
      for (auto i : IntRange<numeric_index_type>(0, training_vector->local_size()))
        {
          numeric_index_type index = first_index+i;
          if (use_log_scaling)
            {
              Real epsilon = 1.e-6; // Prevent rounding errors triggering asserts
              Real log_min   = log10(min_param + epsilon);
              Real log_range = log10( (max_param-epsilon) / (min_param+epsilon) );
              Real step_size = log_range /
                std::max((unsigned int)1,(n_training_samples_in-1));

              if (index<(n_training_samples_in-1))
                {
                  training_vector->set(index, pow(10., log_min + index*step_size ));
                }
              else
                {
                  // due to rounding error, the last parameter can be slightly
                  // bigger than max_parameters, hence snap back to the max
                  training_vector->set(index, max_param);
                }
            }
          else
            {
              // Generate linearly scaled training parameters
              Real step_size = (max_param - min_param) /
                std::max((unsigned int)1,(n_training_samples_in-1));
              training_vector->set(index, index*step_size + min_param);
            }
        }
    }


  // This is for two parameters
  if (num_params == 2)
    {
      // First make sure n_training_samples_in is a square number
      unsigned int n_training_parameters_per_var = static_cast<unsigned int>( std::sqrt(static_cast<Real>(n_training_samples_in)) );
      if ((n_training_parameters_per_var*n_training_parameters_per_var) != n_training_samples_in)
        libmesh_error_msg("Error: Number of training parameters = " \
                          << n_training_samples_in \
                          << ".\n" \
                          << "Deterministic training set generation with two parameters requires\n " \
                          << "the number of training parameters to be a perfect square.");

      // make a matrix to store all the parameters, put them in vector form afterwards
      std::vector<std::vector<Real>> training_parameters_matrix(num_params);

      unsigned int i = 0;
      for (const auto & pr : min_parameters)
        {
          const std::string & param_name = pr.first;
          Real min_param = pr.second;
          bool use_log_scaling = log_param_scale[param_name];
          Real max_param = max_parameters.get_value(param_name);

          training_parameters_matrix[i].resize(n_training_parameters_per_var);

          for (unsigned int j=0; j<n_training_parameters_per_var; j++)
            {
              // Generate log10 scaled training parameters
              if (use_log_scaling)
                {
                  Real epsilon = 1.e-6; // Prevent rounding errors triggering asserts
                  Real log_min   = log10(min_param + epsilon);
                  Real log_range = log10( (max_param-epsilon) / (min_param+epsilon) );
                  Real step_size = log_range /
                    std::max((unsigned int)1,(n_training_parameters_per_var-1));

                  if (j<(n_training_parameters_per_var-1))
                    {
                      training_parameters_matrix[i][j] = pow(10., log_min + j*step_size );
                    }
                  else
                    {
                      // due to rounding error, the last parameter can be slightly
                      // bigger than max_parameters, hence snap back to the max
                      training_parameters_matrix[i][j] = max_param;
                    }
                }
              else
                {
                  // Generate linearly scaled training parameters
                  Real step_size = (max_param - min_param) /
                    std::max((unsigned int)1,(n_training_parameters_per_var-1));
                  training_parameters_matrix[i][j] = j*step_size + min_param;
                }

            }
          i++;
        }

      // now load into training_samples_in:
      std::map<std::string, std::unique_ptr<NumericVector<Number>>>::iterator new_it = training_parameters_in.begin();

      NumericVector<Number> * training_vector_0 = new_it->second.get();
      ++new_it;
      NumericVector<Number> * training_vector_1 = new_it->second.get();

      for (unsigned int index1=0; index1<n_training_parameters_per_var; index1++)
        {
          for (unsigned int index2=0; index2<n_training_parameters_per_var; index2++)
            {
              unsigned int index = index1*n_training_parameters_per_var + index2;

              if ((training_vector_0->first_local_index() <= index) &&
                  (index < training_vector_0->last_local_index()))
                {
                  training_vector_0->set(index, training_parameters_matrix[0][index1]);
                  training_vector_1->set(index, training_parameters_matrix[1][index2]);
                }
            }
        }

      //     libMesh::out << "n_training_samples = " << n_training_samples_in << std::endl;
      //     for (unsigned int index=0; index<n_training_samples_in; index++)
      //     {
      //         libMesh::out << "training parameters for index="<<index<<":"<<std::endl;
      //         for (unsigned int param=0; param<num_params; param++)
      //         {
      //           libMesh::out << " " << (*training_parameters_in[param])(index);
      //         }
      //         libMesh::out << std::endl << std::endl;
      //     }

    }
}


template <class Base>
void RBConstructionBase<Base>::broadcast_parameters(unsigned int proc_id)
{
  libmesh_assert_less (proc_id, this->n_processors());

  // create a copy of the current parameters
  RBParameters current_parameters = get_parameters();

  // copy current_parameters to current_parameters_vector in order to broadcast
  std::vector<Real> current_parameters_vector;

  for (const auto & pr : current_parameters)
    current_parameters_vector.push_back(pr.second);

  // do the broadcast
  this->comm().broadcast(current_parameters_vector, proc_id);

  // update the copy of the RBParameters object
  unsigned int count = 0;
  for (const auto & pr : current_parameters)
    {
      const std::string & param_name = pr.first;
      current_parameters.set_value(param_name, current_parameters_vector[count]);
      count++;
    }

  // set the parameters globally
  set_parameters(current_parameters);
}

template <class Base>
void RBConstructionBase<Base>::set_training_random_seed(unsigned int seed)
{
  this->training_parameters_random_seed = seed;
}

// Template specializations

// EigenSystem is only defined if we have SLEPc
#if defined(LIBMESH_HAVE_SLEPC)
template class RBConstructionBase<CondensedEigenSystem>;
#endif

template class RBConstructionBase<LinearImplicitSystem>;

} // namespace libMesh
