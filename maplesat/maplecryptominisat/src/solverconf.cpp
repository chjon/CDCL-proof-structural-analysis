/******************************************
Copyright (c) 2014, Mate Soos

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
***********************************************/

#include "constants.h"
#include "solverconf.h"
#include <limits>
#include <iomanip>
#include <sstream>
using namespace CMSat;

DLL_PUBLIC SolverConf::SolverConf() :
        //Variable activities
        var_inc_start(1)
        , var_decay_start(0.8)
        , var_decay_max(0.95)
        , random_var_freq(0)
        , polarity_mode(PolarityMode::polarmode_automatic)
        , do_calc_polarity_first_time(true)
        , do_calc_polarity_every_time(false)

        //Clause cleaning
        , max_temporary_learnt_clauses(20000)
        , protect_clause_if_imrpoved_glue_below_this_glue_for_one_turn(30)
        , clean_confl_multiplier(0.2)
        , clean_prop_multiplier(1.0)
        , doPreClauseCleanPropAndConfl(false)
        , preClauseCleanLimit(2)
        , doClearStatEveryClauseCleaning(true)
        , inc_max_temp_red_cls(1.0)
        , maxNumRedsRatio(10)
        , clauseDecayActivity(1.0/0.999)
        , min_time_in_db_before_eligible_for_cleaning(5ULL*1000ULL)
        , glue_must_keep_clause_if_below_or_eq(5)

        //Restarting
        , restart_first(300)
        , restart_inc(2)
        , burst_search_len(300)
        , restartType(Restart::glue)
        , do_blocking_restart(1)
        , blocking_restart_trail_hist_length(5000)
        , blocking_restart_multip(1.4)
        , local_glue_multiplier(0.80)
        , shortTermHistorySize (50)
        , lower_bound_for_blocking_restart(10000)
        , more_otf_shrink_with_cache(false)
        , more_otf_shrink_with_stamp(false)

        //Clause minimisation
        , doRecursiveMinim (true)
        , doMinimRedMore(true)
        , doAlwaysFMinim   (false)
        , max_glue_more_minim(6)
        , max_size_more_minim(30)
        , more_red_minim_limit_cache(400)
        , more_red_minim_limit_binary(200)
        , max_num_lits_more_red_min(1)
        , extra_bump_var_activities_based_on_glue(true)

        //Verbosity
        , verbosity        (0)
        , doPrintGateDot   (false)
        , doPrintConflDot  (false)
        , print_all_stats   (false)
        , verbStats        (0)
        , doPrintBestRedClauses(0)
        , do_print_times(1)
        , print_restart_line_every_n_confl(7000)

        //Limits
        , maxTime          (std::numeric_limits<double>::max())
        , maxConfl         (std::numeric_limits<long>::max())

        //Glues
        , update_glues_on_prop(false)
        , update_glues_on_analyze(true)

        //OTF
        , otfHyperbin      (true)
        , doOTFSubsume     (true)
        , doOTFSubsumeOnlyAtOrBelowGlue(5)
        , rewardShortenedClauseWithConfl(3)

        //SQL
        , doSQL          (1)
        , whichSQL       (0)
        , dump_individual_search_time(false)
        , sqlite_filename ("cryptominisat.sqlite")
        , sqlServer ("localhost")
        , sqlUser ("cmsat_solver")
        , sqlPass ("")
        , sqlDatabase("cmsat")

        //Var-elim
        , doVarElim        (true)
        , varelim_cutoff_too_many_clauses(2000)
        , do_empty_varelim (true)
        , empty_varelim_time_limitM(300LL)
        , varelim_time_limitM(50)
        , updateVarElimComplexityOTF(true)
        , updateVarElimComplexityOTF_limitvars(200)
        , updateVarElimComplexityOTF_limitavg(40ULL*1000ULL)
        , var_elim_strategy  (ElimStrategy::heuristic)
        , varElimCostEstimateStrategy(0)
        , varElimRatioPerIter(0.70)
        , skip_some_bve_resolvents(true)
        , velim_resolvent_too_large(20)

        //Subs, str limits for simplifier
        , subsumption_time_limitM(300)
        , strengthening_time_limitM(300)
        , aggressive_elim_time_limitM(300)

        //Bounded variable addition
        , do_bva(true)
        , bva_limit_per_call(150000)
        , bva_also_twolit_diff(true)
        , bva_extra_lit_and_red_start(0)
        , bva_time_limitM(100)

        //Probing
        , doProbe          (true)
        , doIntreeProbe    (true)
        , probe_bogoprops_time_limitM  (800ULL)
        , intree_time_limitM(400ULL)
        , intree_scc_varreplace_time_limitM(30ULL)
        , doBothProp       (true)
        , doTransRed       (true)
        , doStamp          (true)
        , doCache          (true)
        , cacheUpdateCutoff(2000)
        , maxCacheSizeMB   (2048)
        , otf_hyper_time_limitM(340)
        , otf_hyper_ratio_limit(0.5) //if higher(closer to 1), we allow for less hyper-bin addition, i.e. we are stricter
        , single_probe_time_limit_perc(0.5)

        //XOR
        , doFindXors       (true)
        , maxXorToFind     (5)
        , useCacheWhenFindingXors(false)
        , doEchelonizeXOR  (true)
        , maxXORMatrix     (10LL*1000LL*1000LL)
        , xor_finder_time_limitM(60)

        //Var-replacer
        , doFindAndReplaceEqLits(true)
        , doExtendedSCC         (true)
        , sccFindPercent        (0.04)

        //Propagation & search
        , propBinFirst     (false)
        , dominPickFreq    (0)

        //Iterative Alo Scheduling
        , simplify_at_startup(true)
        , simplify_at_every_startup(false)
        , do_simplify_problem(true)
        , full_simplify_at_startup(false)
        , never_stop_search(false)
        , num_conflicts_of_search(50ULL*1000ULL)
        , num_conflicts_of_search_inc(1.4)
        , simplify_at_startup_sequence("sub-impl, simplify, scc-vrepl")
        , simplify_nonstartup_sequence("handle-comps,"
        "scc-vrepl, cache-clean, cache-tryboth,"
        "sub-impl, intree-probe, probe,"
        "str-cls, distill-cls, scc-vrepl, sub-impl, simplify,"
        "str-impl, cache-clean, str-cls, distill-cls, scc-vrepl,"
        "check-cache-size, renumber"
        )

        //Occur based simplification
        , perform_occur_based_simp(true)
        , do_strengthen_with_occur       (true)
        , maxRedLinkInSize (200)
        , maxOccurIrredMB  (800)
        , maxOccurRedMB    (800)
        , maxOccurRedLitLinkedM(50)
        , subsume_gothrough_multip(4.0)
        , occsimp_schedule_nonstartup("backw-subsume, xor, prop,"
        "clean-implicit, bve, prop,"
        "bva, gates, backw-subsume")
        , occsimp_schedule_startup("backw-subsume, prop,"
        "clean-implicit, bve, prop,"
        "backw-subsume")

        //Distillation
        , do_distill_clauses(true)
        , distill_long_irred_cls_time_limitM(10ULL)
        , watch_cache_stamp_based_str_time_limitM(30LL)
        , distill_time_limitM(120LL)

        //Memory savings
        , doRenumberVars   (true)
        , doSaveMem        (true)

        //Component finding
        , doFindComps     (false)
        , doCompHandler    (true)
        , handlerFromSimpNum (0)
        , compVarLimit      (1ULL*1000ULL*1000ULL)
        , comp_find_time_limitM (500)

        //Misc optimisations
        , doExtBinSubs     (true)
        , doSortWatched    (true)
        , doStrSubImplicit (true)
        , subsume_implicit_time_limitM(30LL)
        , strengthen_implicit_time_limitM(50LL)
        , doCalcReach      (true)

        //Gates
        , doGateFind       (false)
        , maxGateBasedClReduceSize(20)
        , doShortenWithOrGates(true)
        , doRemClWithAndGates(true)
        , doFindEqLitsWithGates(true)
        , gatefinder_time_limitM(200)
        , shorten_with_gates_time_limitM(200)
        , remove_cl_with_gates_time_limitM(100)

        //Misc
        , orig_global_timeout_multiplier(1.0)
        , global_timeout_multiplier(1.0)
        , global_timeout_multiplier_multiplier(1.2)
        , maxDumpRedsSize(std::numeric_limits<uint32_t>::max())
        , origSeed(0)
        , sync_every_confl(20000)
        , clean_after_perc_zero_depth_assigns(0.015)
        , reconfigure_val(0)
        , reconfigure_at(2)
{

    ratio_keep_clauses[clean_to_int(ClauseClean::glue)] = 0;
    ratio_keep_clauses[clean_to_int(ClauseClean::size)] = 0;
    ratio_keep_clauses[clean_to_int(ClauseClean::activity)] = 0.5;
}


std::string SolverConf::print_times(
    const double time_used
    , const bool time_out
    , const double time_remain
) const {
    if (do_print_times) {
        std::stringstream ss;
        ss
        << " T: " << std::setprecision(2) << std::fixed << time_used
        << " T-out: " << (time_out ? "Y" : "N")
        << " T-r: " << time_remain*100.0  << "%";

        return ss.str();
    }

    return std::string();
}

std::string SolverConf::print_times(
    const double time_used
    , const bool time_out
) const {
    if (do_print_times) {
        std::stringstream ss;
        ss
        << " T: " << std::setprecision(2) << std::fixed << time_used
        << " T-out: " << (time_out ? "Y" : "N");

        return ss.str();
    }

    return std::string();
}

std::string SolverConf::print_times(
    const double time_used
) const {
    if (do_print_times) {
        std::stringstream ss;
        ss
        << " T: " << std::setprecision(2) << std::fixed << time_used;

        return ss.str();
    }

    return std::string();
}