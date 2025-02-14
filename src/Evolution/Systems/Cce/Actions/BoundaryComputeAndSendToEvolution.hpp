// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <string>
#include <tuple>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/VariablesTag.hpp"
#include "Evolution/Systems/Cce/Actions/ReceiveWorldtubeData.hpp"
#include "Evolution/Systems/Cce/Components/WorldtubeBoundary.hpp"
#include "Evolution/Systems/Cce/InterfaceManagers/GhInterfaceManager.hpp"
#include "Evolution/Systems/Cce/OptionTags.hpp"
#include "Evolution/Systems/Cce/ReceiveTags.hpp"
#include "Evolution/Systems/Cce/Tags.hpp"
#include "Evolution/Systems/Cce/WorldtubeDataManager.hpp"
#include "IO/Observer/Actions/GetLockPointer.hpp"
#include "IO/Observer/ObserverComponent.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/Invoke.hpp"
#include "Parallel/Local.hpp"
#include "Time/SelfStart.hpp"
#include "Time/TimeStepId.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TypeTraits.hpp"

namespace Cce {
namespace Actions {

/*!
 * \ingroup ActionsGroup
 * \brief Obtains the CCE boundary data at the specified `time`, and reports it
 * to the `EvolutionComponent` via `Actions::ReceiveWorldtubeData`.
 *
 * \details See the template partial specializations of this class for details
 * on the different strategies for each component type.
 */
template <typename BoundaryComponent, typename EvolutionComponent>
struct BoundaryComputeAndSendToEvolution;

/*!
 * \ingroup ActionsGroup
 * \brief Computes Bondi boundary data from GH evolution variables and sends the
 * result to the `EvolutionComponent` (template argument).
 *
 * \details After the computation, this action will call
 * `Cce::Actions::ReceiveWorldtubeData` on the `EvolutionComponent` with each of
 * the types from `typename Metavariables::cce_boundary_communication_tags` sent
 * as arguments
 *
 * \ref DataBoxGroup changes:
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies:
 *   - `Tags::Variables<typename
 *   Metavariables::cce_boundary_communication_tags>` (every tensor)
 */
template <typename BoundaryComponent, typename EvolutionComponent>
struct SendToEvolution;

/*!
 * \ingroup ActionsGroup
 * \brief Obtains the CCE boundary data at the specified `time`, and reports it
 * to the `EvolutionComponent` via `Actions::ReceiveWorldtubeData`.
 *
 * \details This uses the `WorldtubeDataManager` to perform all of the work of
 * managing the file buffer, interpolating to the desired time point, and
 * compute the Bondi quantities on the boundary. Once readied, it sends each
 * tensor from the the full `Variables<typename
 * Metavariables::cce_boundary_communication_tags>` back to the
 * `EvolutionComponent`
 *
 * Uses:
 * - DataBox:
 *  - `Tags::H5WorldtubeBoundaryDataManager`
 *
 * \ref DataBoxGroup changes:
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies:
 *   - `Tags::Variables<typename
 * Metavariables::cce_boundary_communication_tags>` (every tensor)
 */
template <typename Metavariables, typename EvolutionComponent>
struct BoundaryComputeAndSendToEvolution<H5WorldtubeBoundary<Metavariables>,
                                         EvolutionComponent> {
  template <typename ParallelComponent, typename... DbTags, typename ArrayIndex>
  static void apply(db::DataBox<tmpl::list<DbTags...>>& box,
                    Parallel::GlobalCache<Metavariables>& cache,
                    const ArrayIndex& /*array_index*/, const TimeStepId& time) {
    auto hdf5_lock = Parallel::local_branch(
                         Parallel::get_parallel_component<
                             observers::ObserverWriter<Metavariables>>(cache))
                         ->template local_synchronous_action<
                             observers::Actions::GetLockPointer<
                                 observers::Tags::H5FileLock>>();
    bool successfully_populated = false;
    db::mutate<Tags::H5WorldtubeBoundaryDataManager,
               ::Tags::Variables<
                   typename Metavariables::cce_boundary_communication_tags>>(
        [&successfully_populated, &time, &hdf5_lock](
            const gsl::not_null<std::unique_ptr<Cce::WorldtubeDataManager<
                Tags::characteristic_worldtube_boundary_tags<
                    Tags::BoundaryValue>>>*>
                worldtube_data_manager,
            const gsl::not_null<Variables<
                typename Metavariables::cce_boundary_communication_tags>*>
                boundary_variables) {
          successfully_populated =
              (*worldtube_data_manager)
                  ->populate_hypersurface_boundary_data(boundary_variables,
                                                        time.substep_time(),
                                                        hdf5_lock);
        },
        make_not_null(&box));
    if (not successfully_populated) {
      ERROR("Insufficient boundary data to proceed, exiting early at time " +
            std::to_string(time.substep_time()));
    }
    Parallel::receive_data<Cce::ReceiveTags::BoundaryData<
        typename Metavariables::cce_boundary_communication_tags>>(
        Parallel::get_parallel_component<EvolutionComponent>(cache), time,
        db::get<::Tags::Variables<
            typename Metavariables::cce_boundary_communication_tags>>(box),
        true);
  }
};

/*!
 * \ingroup ActionsGroup
 * \brief Obtains the Klein-Gordon CCE boundary data at the specified `time`,
 * and reports it to the `EvolutionComponent` via
 * `Actions::ReceiveWorldtubeData`.
 *
 * \details This uses the `WorldtubeDataManager` to perform all of the work of
 * managing the file buffer, interpolating to the desired time point, and
 * compute the Bondi and Klein-Gordon quantities on the boundary. Once readied,
 * it sends each tensor or scalar from the the full `Variables<typename
 * Metavariables::cce_boundary_communication_tags>` or `Variables<typename
 * Metavariables::klein_gordon_boundary_communication_tags>` back to the
 * `EvolutionComponent`
 *
 * Uses:
 * - DataBox:
 *  - `Tags::H5WorldtubeBoundaryDataManager`
 *  - `Tags::KleinGordonH5WorldtubeBoundaryDataManager`
 *
 * \ref DataBoxGroup changes:
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies:
 *   - `Tags::Variables<typename
 * Metavariables::cce_boundary_communication_tags>` (every tensor)
 *   - `Tags::Variables<typename
 * Metavariables::klein_gordon_boundary_communication_tags>` (every scalar)
 */
template <typename Metavariables, typename EvolutionComponent>
struct BoundaryComputeAndSendToEvolution<
    KleinGordonH5WorldtubeBoundary<Metavariables>, EvolutionComponent> {
  template <typename ParallelComponent, typename... DbTags, typename ArrayIndex>
  static void apply(db::DataBox<tmpl::list<DbTags...>>& box,
                    Parallel::GlobalCache<Metavariables>& cache,
                    const ArrayIndex& /*array_index*/, const TimeStepId& time) {
    auto hdf5_lock = Parallel::local_branch(
                         Parallel::get_parallel_component<
                             observers::ObserverWriter<Metavariables>>(cache))
                         ->template local_synchronous_action<
                             observers::Actions::GetLockPointer<
                                 observers::Tags::H5FileLock>>();
    bool tensor_successfully_populated = false;
    bool klein_gordon_successfully_populated = false;
    db::mutate<
        Tags::H5WorldtubeBoundaryDataManager,
        Tags::KleinGordonH5WorldtubeBoundaryDataManager,
        ::Tags::Variables<
            typename Metavariables::cce_boundary_communication_tags>,
        ::Tags::Variables<
            typename Metavariables::klein_gordon_boundary_communication_tags>>(
        [&tensor_successfully_populated, &klein_gordon_successfully_populated,
         &time, &hdf5_lock](
            const gsl::not_null<std::unique_ptr<Cce::WorldtubeDataManager<
                Tags::characteristic_worldtube_boundary_tags<
                    Tags::BoundaryValue>>>*>
                tensor_worldtube_data_manager,
            const gsl::not_null<std::unique_ptr<Cce::WorldtubeDataManager<
                Tags::klein_gordon_worldtube_boundary_tags>>*>
                klein_gordon_worldtube_data_manager,
            const gsl::not_null<Variables<
                typename Metavariables::cce_boundary_communication_tags>*>
                tensor_boundary_variables,
            const gsl::not_null<
                Variables<typename Metavariables::
                              klein_gordon_boundary_communication_tags>*>
                klein_gordon_boundary_variables) {
          tensor_successfully_populated =
              (*tensor_worldtube_data_manager)
                  ->populate_hypersurface_boundary_data(
                      tensor_boundary_variables, time.substep_time(),
                      hdf5_lock);

          klein_gordon_successfully_populated =
              (*klein_gordon_worldtube_data_manager)
                  ->populate_hypersurface_boundary_data(
                      klein_gordon_boundary_variables, time.substep_time(),
                      hdf5_lock);
        },
        make_not_null(&box));
    if (not tensor_successfully_populated) {
      ERROR(
          "Insufficient tensor boundary data to proceed, exiting early at "
          "time " +
          std::to_string(time.substep_time()));
    }

    if (not klein_gordon_successfully_populated) {
      ERROR(
          "Insufficient scalar boundary data to proceed, exiting early at "
          "time " +
          std::to_string(time.substep_time()));
    }
    Parallel::receive_data<Cce::ReceiveTags::BoundaryData<
        typename Metavariables::cce_boundary_communication_tags>>(
        Parallel::get_parallel_component<EvolutionComponent>(cache), time,
        db::get<::Tags::Variables<
            typename Metavariables::cce_boundary_communication_tags>>(box),
        true);

    Parallel::receive_data<Cce::ReceiveTags::BoundaryData<
        typename Metavariables::klein_gordon_boundary_communication_tags>>(
        Parallel::get_parallel_component<EvolutionComponent>(cache), time,
        db::get<::Tags::Variables<
            typename Metavariables::klein_gordon_boundary_communication_tags>>(
            box),
        true);
  }
};

/*!
 * \ingroup ActionsGroup
 * \brief Calculates the analytic boundary data at the specified `time`, and
 * sends the resulting Bondi-Sachs boundary data to the `EvolutionComponent`
 *
 * \details This uses the `Cce::AnalyticBoundaryDataManager` to
 * perform all of the work of calculating the analytic boundary solution, which
 * in turn uses derived classes of `Cce::Solutions::WorldtubeData` to calculate
 * the metric data before it is transformed to Bondi-Sachs variables.
 *
 * \ref DataBoxGroup changes:
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies:
 *   - `Tags::AnalyticWordltubeBoundaryDataManager`
 */
template <typename Metavariables, typename EvolutionComponent>
struct BoundaryComputeAndSendToEvolution<
    AnalyticWorldtubeBoundary<Metavariables>, EvolutionComponent> {
  template <typename ParallelComponent, typename DbTagList, typename ArrayIndex>
  static void apply(db::DataBox<DbTagList>& box,
                    Parallel::GlobalCache<Metavariables>& cache,
                    const ArrayIndex& /*array_index*/, const TimeStepId& time) {
    bool successfully_populated = false;
    db::mutate<Tags::AnalyticBoundaryDataManager,
               ::Tags::Variables<
                   typename Metavariables::cce_boundary_communication_tags>>(
        [&successfully_populated, &time](
            const gsl::not_null<Cce::AnalyticBoundaryDataManager*>
                worldtube_data_manager,
            const gsl::not_null<Variables<
                typename Metavariables::cce_boundary_communication_tags>*>
                boundary_variables) {
          successfully_populated =
              (*worldtube_data_manager)
                  .populate_hypersurface_boundary_data(boundary_variables,
                                                       time.substep_time());
        },
        make_not_null(&box));

    if (not successfully_populated) {
      ERROR("Insufficient boundary data to proceed, exiting early at time "
            << time.substep_time());
    }
    Parallel::receive_data<Cce::ReceiveTags::BoundaryData<
        typename Metavariables::cce_boundary_communication_tags>>(
        Parallel::get_parallel_component<EvolutionComponent>(cache), time,
        db::get<::Tags::Variables<
            typename Metavariables::cce_boundary_communication_tags>>(box),
        true);
  }
};

/*!
 * \ingroup ActionsGroup
 * \brief Submits a request for CCE boundary data at the specified `time` to the
 * `Cce::InterfaceManagers::GhInterfaceManager`, and sends the data to the
 * `EvolutionComponent` (template argument) if it is ready.
 *
 * \details This uses the `Cce::InterfaceManagers::GhInterfaceManager` to
 * perform all of the work of managing the buffer of data sent from the GH
 * system and interpolating if necessary and supported. This dispatches then to
 * `Cce::Actions::SendToEvolution<GhWorldtubeBoundary<Metavariables>,
 * EvolutionComponent>` if the boundary data is ready, otherwise
 * simply submits the request and waits for data to become available via
 * `Cce::Actions::ReceiveGhWorldtubeData`, which will call
 * `Cce::Actions::SendToEvolution<GhWorldtubeBoundary<Metavariables>,
 * EvolutionComponent>` as soon as the data becomes available.
 *
 * \ref DataBoxGroup changes:
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies:
 *   - `Tags::GhInterfaceManager`
 */
template <typename Metavariables, typename EvolutionComponent>
struct BoundaryComputeAndSendToEvolution<GhWorldtubeBoundary<Metavariables>,
                                         EvolutionComponent> {
  template <typename ParallelComponent, typename... DbTags, typename ArrayIndex>
  static void apply(db::DataBox<tmpl::list<DbTags...>>& box,
                    Parallel::GlobalCache<Metavariables>& cache,
                    const ArrayIndex& /*array_index*/, const TimeStepId& time) {
    auto retrieve_data_and_send_to_evolution =
        [&time,
         &cache](const gsl::not_null<InterfaceManagers::GhInterfaceManager*>
                     interface_manager) {
          interface_manager->request_gh_data(time);
          const auto gh_data =
              interface_manager->retrieve_and_remove_first_ready_gh_data();
          if (static_cast<bool>(gh_data)) {
            Parallel::simple_action<Actions::SendToEvolution<
                GhWorldtubeBoundary<Metavariables>, EvolutionComponent>>(
                Parallel::get_parallel_component<
                    GhWorldtubeBoundary<Metavariables>>(cache),
                get<0>(*gh_data), get<1>(*gh_data));
          }
        };
    if (SelfStart::is_self_starting(time)) {
      db::mutate<Tags::SelfStartGhInterfaceManager>(
          retrieve_data_and_send_to_evolution, make_not_null(&box));
    } else {
      db::mutate<Tags::GhInterfaceManager>(retrieve_data_and_send_to_evolution,
                                           make_not_null(&box));
    }
  }
};

/// \cond
template <typename Metavariables, typename EvolutionComponent>
struct SendToEvolution<GhWorldtubeBoundary<Metavariables>, EvolutionComponent> {
  template <typename ParallelComponent, typename... DbTags, typename ArrayIndex>
  static void apply(
      db::DataBox<tmpl::list<DbTags...>>& box,
      Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& array_index, const TimeStepId& time,
      const InterfaceManagers::GhInterfaceManager::gh_variables& gh_variables) {
    apply<ParallelComponent>(
        box, cache, array_index, time,
        get<gr::Tags::SpacetimeMetric<DataVector, 3>>(gh_variables),
        get<gh::Tags::Phi<DataVector, 3>>(gh_variables),
        get<gh::Tags::Pi<DataVector, 3>>(gh_variables));
  }

  template <typename ParallelComponent, typename... DbTags, typename ArrayIndex>
  static void apply(
      db::DataBox<tmpl::list<DbTags...>>& box,
      Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& /*array_index*/, const TimeStepId& time,
      const tnsr::aa<DataVector, 3, ::Frame::Inertial>& spacetime_metric,
      const tnsr::iaa<DataVector, 3, ::Frame::Inertial>& phi,
      const tnsr::aa<DataVector, 3, ::Frame::Inertial>& pi) {
    db::mutate<::Tags::Variables<
        typename Metavariables::cce_boundary_communication_tags>>(
        [&spacetime_metric, &phi, &pi](
            const gsl::not_null<Variables<
                typename Metavariables::cce_boundary_communication_tags>*>
                boundary_variables,
            const double extraction_radius, const double l_max) {
          create_bondi_boundary_data(boundary_variables, phi, pi,
                                     spacetime_metric, extraction_radius,
                                     l_max);
        },
        make_not_null(&box), db::get<InitializationTags::ExtractionRadius>(box),
        db::get<Tags::LMax>(box));
    Parallel::receive_data<Cce::ReceiveTags::BoundaryData<
        typename Metavariables::cce_boundary_communication_tags>>(
        Parallel::get_parallel_component<EvolutionComponent>(cache), time,
        db::get<::Tags::Variables<
            typename Metavariables::cce_boundary_communication_tags>>(box),
        true);
  }
};
/// \endcond

}  // namespace Actions
}  // namespace Cce
