#pragma once

//#include <ADEKF/ceres/jet.h>
#include "ADEKFUtils.h"
#include "ADEKF.h"

#include <iostream>
#include <Eigen/Core>
#include <vector>

namespace adekf
{

    //using namespace Eigen;
    //using namespace std::placeholders;

    /**
     * An RTS Smoother Implementation based on the ADEKF
     * @tparam Filter The Filter to be used for estimation
     */
    template <typename State>
    class RTS_Smoother : public ADEKF<State>
    {

    public:
        using Covariance = typename ADEKF<State>::Covariance;
        using ScalarType = typename ADEKF<State>::ScalarType;
        static constexpr size_t DOF = DOFOf<State>;
        static constexpr double weighted_mean_epsilon = 1e-8;
        aligned_vector<State> old_mus, smoothed_mus, predicted_mus;
        aligned_vector<Covariance> old_sigmas, smoothed_sigmas, predicted_sigmas;

        RTS_Smoother(const State &_mu, const Covariance &_sigma) : ADEKF<State>(_mu, _sigma)
        {
            storeEstimation();
            storePredictedEstimation();//called only to have something in 0. Is never used.
        }
        RTS_Smoother(const ADEKF<State> &filter) : ADEKF<State>(filter)
        {
            storeEstimation();
            storePredictedEstimation();//called only to have something in 0. Is never used.
        }
        /**
         * Stores the current mean and covariance of the filter 
         */
        void storeEstimation()
        {
            old_mus.push_back(this->mu);
            smoothed_mus.push_back(this->mu);
            old_sigmas.push_back(this->sigma);
            smoothed_sigmas.push_back(this->sigma);
        }

        void storePredictedEstimation()
        {
            predicted_mus.push_back(this->mu);
            predicted_sigmas.push_back(this->sigma);
        }
        template <typename Head, typename... Tail>
        std::tuple<Tail...> tuple_tail(const std::tuple<Head, Tail...> &tuple)
        {
            return apply([](auto head, auto... tail) {
                return std::make_tuple(tail...);
            },
                         tuple);
        }

        template <typename DynamicModel, typename tstate, typename NoiseVector, typename... Controls, typename... ExpandedControls>
        void applyDynamicModel(DynamicModel dynamicModel, tstate &input, const NoiseVector &noise, const std::tuple<Controls...> &controls, const ExpandedControls &...expanded_controls)
        {
            if constexpr (std::tuple_size<std::tuple<Controls...>>::value > 0)
            {
                applyDynamicModel(dynamicModel, input, noise, tuple_tail(controls), expanded_controls..., std::get<0>(controls));
            }
            else
            {
                dynamicModel(input, noise, expanded_controls...);
            }
        }

    

        template <int NoiseDim, typename DynamicModel, typename... Controls>
        Covariance getNonAdditiveDynamicNoise(const State &smoothed, DynamicModel dynamicModel, const Eigen::Matrix<double,NoiseDim,NoiseDim> &Q, const std::tuple<Controls...> &controls)
        {
            auto derivator = adekf::getDerivator<DOF + NoiseDim>();
            auto input = eval(smoothed + derivator.template head<DOF>());
            //Todo u vernünftig machen
            applyDynamicModel(dynamicModel, input, derivator.template tail<NoiseDim>(), controls);
            auto result = (input - smoothed);
            //std::cout << result << std::endl;
            auto F=extractJacobi(result);
            return eval(F.template rightCols<NoiseDim>() * Q * F.template rightCols<NoiseDim>().transpose());
        }
       
        

static inline size_t indefinite_counter=0;
        template <int NoiseDim, typename DynamicModel, typename... Controls>
        void smoothSingleStep(size_t k, DynamicModel &dynamicModel, const Eigen::Matrix<double,NoiseDim,NoiseDim> &Q, const std::tuple<Controls...> &controls, const State &smoothed_mu_kplus, const Covariance &smoothed_sigma_kplus)
        {
         
            auto delta = adekf::getDerivator<DOF>();
            auto input = eval(old_mus[k] + delta);
            applyDynamicModel(dynamicModel, input, MatrixType<ScalarType,NoiseDim, 1>::Zero(), controls);
           
            Covariance Fk=extractJacobi(input - predicted_mus[k+1]);

            Covariance SmootherGain = old_sigmas[k] * Fk.transpose() * predicted_sigmas[k+1].inverse();
            typename State::template DeltaType<double> smoother_innovation=SmootherGain * (smoothed_mu_kplus-predicted_mus[k + 1]);
            smoothed_mus[k] = old_mus[k] + smoother_innovation;

          
            Covariance Jk=transformReferenceJacobian(old_mus[k],smoothed_mus[k],smoother_innovation);
           
            Covariance Bk=transformReferenceJacobian(smoothed_mu_kplus,predicted_mus[k+1]); 
            smoothed_sigmas[k] = Jk * (old_sigmas[k] + SmootherGain * (Bk*smoothed_sigma_kplus*Bk.transpose()-predicted_sigmas[k+1]) * SmootherGain.transpose())*Jk.transpose();
            if(!isPositiveDefinite(smoothed_sigmas[k]))
                indefinite_counter++;
            assurePositiveDefinite(smoothed_sigmas[k]);
            }

        template <int NoiseDim, typename DynamicModel, typename... Controls>
        void smoothIntervalWithNonAdditiveNoise(size_t steps, int start, DynamicModel &dynamicModel, const Eigen::Matrix<double,NoiseDim,NoiseDim> &Q, const std::vector<std::tuple<Controls...>> &all_controls)
        {
            if (start < 0)
            {
                start = old_mus.size() - 1 + start;
            }
            assert(start - steps >= 0 && "Can not smooth more estimates than available");
            assert(start < old_mus.size() - 1 && "Can not smooth a state which belongs to future");
            assert(all_controls.size() == old_mus.size() - 1 && "Requires all control inputs for the dynamic model.");
            for (size_t k = start; k >= old_mus.size() - steps - 1; k--)
            {
                //std::cout << "At iteration: " << k << std::endl;
                smoothSingleStep(k, dynamicModel, Q, all_controls[k], smoothed_mus[k + 1], smoothed_sigmas[k + 1]);
            }
        }

        template <int NoiseDim, typename DynamicModel, typename... Controls>
        void smoothAllWithNonAdditiveNoise(DynamicModel &dynamicModel, const Eigen::Matrix<double,NoiseDim,NoiseDim> &Q, const std::vector<std::tuple<Controls...>> &all_controls)
        {
            smoothIntervalWithNonAdditiveNoise(old_mus.size() - 2, -1, dynamicModel, Q, all_controls);
        }
    };

    /**
     * General Deduction Template for the RTS_Smoother based on StateRetriever.
     * This is needed so you can type RTS_Smoother smoother(Filter{...}) without template arguments
     */
    /*template <typename State>
    RTS_Smoother(ADEKF<State> &&) -> RTS_Smoother<State>;
    template <typename State>
    RTS_Smoother(ADEKF<State> ) -> RTS_Smoother<State>;
*/

} // namespace adekf