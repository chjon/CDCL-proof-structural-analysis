/*
 * CryptoMiniSat
 *
 * Copyright (c) 2009-2015, Mate Soos. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.0 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
*/

#include "bva.h"
#include "occsimplifier.h"
#include "solver.h"
#include "clausecleaner.h"
#include "subsumeimplicit.h"
#include "sqlstats.h"
#include <cmath>
#include <functional>

using namespace CMSat;

BVA::BVA(Solver* _solver, OccSimplifier* _simplifier) :
    solver(_solver)
    , simplifier(_simplifier)
    , seen(solver->seen)
    , seen2(solver->seen2)
    , var_bva_order(VarBVAOrder(watch_irred_sizes))
{}

bool BVA::bounded_var_addition()
{
    bounded_var_elim_time_limit =
        solver->conf.bva_time_limitM*2ULL*400LL *1000LL
        *solver->conf.global_timeout_multiplier;
    bva_verbosity = false;

    assert(solver->ok);
    if (!solver->conf.do_bva)
        return solver->okay();

    if (solver->conf.verbosity >= 3 || bva_verbosity) {
        cout << "c [bva] Running BVA" << endl;
    }

    if (!solver->propagate_occur())
        return false;

    simplifier->limit_to_decrease = &bounded_var_elim_time_limit;
    int64_t limit_orig = *simplifier->limit_to_decrease;
    solver->clauseCleaner->clean_implicit_clauses();
    if (solver->conf.doStrSubImplicit) {
        solver->subsumeImplicit->subsume_implicit(false);
    }

    bva_worked = 0;
    bva_simp_size = 0;
    var_bva_order.clear();
    calc_watch_irred_sizes();
    for(size_t i = 0; i < solver->nVars()*2; i++) {
        const Lit lit = Lit::toLit(i);
        if (solver->value(lit) != l_Undef
            || solver->varData[lit.var()].removed != Removed::none
        ) {
            continue;
        }
        var_bva_order.insert(lit.toInt());
    }

    double my_time = cpuTime();
    while(!var_bva_order.empty()) {
        if (*simplifier->limit_to_decrease < 0
            || bva_worked >= solver->conf.bva_limit_per_call
            || solver->must_interrupt_asap()
        ) {
            break;
        }

        const Lit lit = Lit::toLit(var_bva_order.remove_min());
        if (solver->conf.verbosity >= 5 || bva_verbosity) {
            cout << "c [bva] trying lit " << lit << endl;
        }
        bool ok = try_bva_on_lit(lit);
        if (!ok)
            break;
    }
    solver->bva_changed();

    bool time_out = *simplifier->limit_to_decrease <= 0;
    const double time_used = cpuTime() - my_time;
    double time_remain = calc_percentage(*simplifier->limit_to_decrease ,limit_orig);
    if (solver->conf.verbosity >= 2) {
        cout
        << "c [bva] added: " << bva_worked
        << " simp: " << bva_simp_size
        << " 2lit: " << ((solver->conf.bva_also_twolit_diff
            && (long)solver->sumConflicts() >= solver->conf.bva_extra_lit_and_red_start) ? "Y" : "N")
        << solver->conf.print_times(time_used, time_out, time_remain)
        << endl;
    }
    if (solver->sqlStats) {
        solver->sqlStats->time_passed(
            solver
            , "bva"
            , time_used
            , time_out
            , time_remain
        );
    }

    return solver->okay();
}

void BVA::remove_duplicates_from_m_cls()
{
    if (m_cls.size() <= 1)
        return;

    std::function<bool (const OccurClause&, const OccurClause&)> mysort
        = [&] (const OccurClause& a, const OccurClause& b) {
            WatchType atype = a.ws.getType();
            WatchType btype = b.ws.getType();
            if (atype == watch_binary_t && btype != CMSat::watch_binary_t) {
                return true;
            }
            if (btype == watch_binary_t && atype != CMSat::watch_binary_t) {
                return false;
            }
            if (atype == watch_tertiary_t && btype != CMSat::watch_tertiary_t) {
                return true;
            }
            if (btype == watch_tertiary_t && atype != CMSat::watch_tertiary_t) {
                return false;
            }

            assert(atype == btype);
            switch(atype) {
                case CMSat::watch_binary_t: {
                    //subsumption could have time-outed
                    //assert(a.ws.lit2() != b.ws.lit2() && "Implicit has been cleaned of duplicates!!");
                    return a.ws.lit2() < b.ws.lit2();
                }
                case CMSat::watch_tertiary_t: {
                    if (a.ws.lit2() != b.ws.lit2()) {
                        return a.ws.lit2() < b.ws.lit2();
                    }
                    //subsumption could have time-outed
                    //assert(a.ws.lit3() != b.ws.lit3() && "Implicit has been cleaned of duplicates!!");
                    return a.ws.lit3() < b.ws.lit3();
                }
                case CMSat::watch_clause_t: {
                    *simplifier->limit_to_decrease -= 20;
                    const Clause& cl_a = *solver->cl_alloc.ptr(a.ws.get_offset());
                    const Clause& cl_b = *solver->cl_alloc.ptr(b.ws.get_offset());
                    if (cl_a.size() != cl_b.size()) {
                        return cl_a.size() < cl_b.size();
                    }
                    //Clauses' lits are sorted, yay!
                    for(size_t i = 0; i < cl_a.size(); i++) {
                        *simplifier->limit_to_decrease -= 1;
                        if (cl_a[i] != cl_b[i]) {
                            return cl_a[i] < cl_b[i];
                        }
                    }
                    return false;
                }
            }

            assert(false);
            return false;
    };

    *simplifier->limit_to_decrease -= 2*(long)m_cls.size()*(long)std::sqrt(m_cls.size());
    std::sort(m_cls.begin(), m_cls.end(), mysort);
    size_t i = 0;
    size_t j = 0;
    for(; i+1 < m_cls.size(); i++) {
        const Watched& prev = m_cls[j].ws;
        const Watched& next = m_cls[i+1].ws;
        if (prev.getType() != next.getType()) {
            m_cls[j+1] = m_cls[i+1];
            j++;
            continue;
        }

        bool del = false;
        switch(prev.getType()) {
            case CMSat::watch_binary_t: {
                if (prev.lit2() == next.lit2()) {
                    del = true;
                }
                break;
            }

            case CMSat::watch_tertiary_t: {
                if (prev.lit2() == next.lit2() && prev.lit3() == next.lit3())
                    del = true;
                break;
            }

            case CMSat::watch_clause_t: {
                *simplifier->limit_to_decrease -= 10;
                const Clause& cl1 = *solver->cl_alloc.ptr(prev.get_offset());
                const Clause& cl2 = *solver->cl_alloc.ptr(next.get_offset());
                del = true;
                if (cl1.size() != cl2.size()) {
                    break;
                }
                for(size_t at = 0; at < cl1.size(); at++) {
                    *simplifier->limit_to_decrease -= 1;
                    if (cl1[at] != cl2[at]) {
                        del = false;
                        break;
                    }
                }
                break;
            }
        }

        if (!del) {
            m_cls[j+1] = m_cls[i+1];
            //if (mark_irred) {
            //    m_cls[j+1].ws.setRed(false);
            //}
            j++;
        }
    }
    m_cls.resize(m_cls.size()-(i-j));

    if (solver->conf.verbosity >= 6 || bva_verbosity) {
        cout << "m_cls after cleaning: " << endl;
        for(const OccurClause& w: m_cls) {
            cout << "-> " << solver->watched_to_string(w.lit, w.ws) << endl;
        }
    }
}

bool BVA::try_bva_on_lit(const Lit lit)
{
    assert(solver->value(lit) == l_Undef);
    assert(solver->varData[lit.var()].removed == Removed::none);

    m_cls.clear();
    m_lits.clear();
    m_lits.push_back(lit);
    *simplifier->limit_to_decrease -= solver->watches[lit.toInt()].size();
    for(const Watched w: solver->watches[lit.toInt()]) {
        if (!solver->redundant(w)) {
            m_cls.push_back(OccurClause(lit, w));
            if (solver->conf.verbosity >= 6 || bva_verbosity) {
                cout << "1st adding to m_cls "
                << solver->watched_to_string(lit, w)
                << endl;
            }
        }
    }
    remove_duplicates_from_m_cls();

    while(true) {
        potential.clear();
        fill_potential(lit);
        if (*simplifier->limit_to_decrease < 0) {
            return solver->okay();
        }

        size_t num_occur;
        const lit_pair l_max = most_occuring_lit_in_potential(num_occur);
        if (simplifies_system(num_occur)) {
            m_lits.push_back(l_max);
            m_cls.clear();
            *simplifier->limit_to_decrease -= potential.size()*3;
            for(const PotentialClause pot: potential) {
                if (pot.lits == l_max) {
                    m_cls.push_back(pot.occur_cl);
                    if (solver->conf.verbosity >= 6 || bva_verbosity) {
                        cout << "-- max is : (" << l_max.lit1 << ", " << l_max.lit2 << "), adding to m_cls "
                        << solver->watched_to_string(pot.occur_cl.lit, pot.occur_cl.ws)
                        << endl;
                    }
                    assert(pot.occur_cl.lit == lit);
                }
            }
        } else {
            break;
        }
    }

    const int simp_size = simplification_size(m_lits.size(), m_cls.size());
    if (simp_size <= 0) {
        return solver->okay();
    }

    const bool ok = bva_simplify_system();
    return ok;
}

bool BVA::bva_simplify_system()
{
    touched.clear();
    int simp_size = simplification_size(m_lits.size(), m_cls.size());
    if (solver->conf.verbosity >= 6 || bva_verbosity) {
        cout
        << "c [bva] YES Simplification by "
        << simp_size
        << " with matching lits: ";
        for(const lit_pair l: m_lits) {
            cout << "(" << l.lit1;
            if (l.lit2 != lit_Undef) {
                cout << ", " << l.lit2;
            }
            cout << "), ";
        }
        cout << endl;
        cout << "c [bva] cls: ";
        for(OccurClause cl: m_cls) {
            cout
            << "(" << solver->watched_to_string(cl.lit, cl.ws) << ")"
            << ", ";
        }
        cout << endl;
    }
    bva_worked++;
    bva_simp_size += simp_size;

    solver->new_var(true);
    const Var newvar = solver->nVars()-1;
    const Lit new_lit(newvar, false);

    //Binary clauses
    for(const lit_pair m_lit: m_lits) {
        bva_tmp_lits.clear();
        bva_tmp_lits.push_back(m_lit.lit1);
        if (m_lit.lit2 != lit_Undef) {
            bva_tmp_lits.push_back(m_lit.lit2);
        }
        bva_tmp_lits.push_back(new_lit);
        solver->add_clause_int(bva_tmp_lits, false, ClauseStats(), false, &bva_tmp_lits, true, new_lit);
        touched.touch(bva_tmp_lits);
    }

    //Longer clauses
    for(const OccurClause m_cl: m_cls) {
        bool ok = add_longer_clause(~new_lit, m_cl);
        if (!ok)
            return false;
    }

    fill_m_cls_lits_and_red();
    for(const lit_pair replace_lit: m_lits) {
       //cout << "Doing lit " << replace_lit << " replacing lit " << lit << endl;
        for(const m_cls_lits_and_red& cl_lits_and_red: m_cls_lits) {
            remove_matching_clause(cl_lits_and_red, replace_lit);
        }
    }

    update_touched_lits_in_bva();

    return solver->okay();
}

void BVA::update_touched_lits_in_bva()
{
    const vector<uint32_t>& touched_list = touched.getTouchedList();
    for(const uint32_t lit_uint: touched_list) {
        const Lit lit = Lit::toLit(lit_uint);
        if (var_bva_order.in_heap(lit.toInt())) {
            watch_irred_sizes[lit.toInt()] = calc_watch_irred_size(lit);
            var_bva_order.update_if_inside(lit.toInt());
        }

        if (var_bva_order.in_heap((~lit).toInt())) {
            watch_irred_sizes[(~lit).toInt()] = calc_watch_irred_size(~lit);
            var_bva_order.update_if_inside((~lit).toInt());
        }
    }
    touched.clear();
}

void BVA::fill_m_cls_lits_and_red()
{
    m_cls_lits.clear();
    vector<Lit> tmp;
    for(OccurClause& cl: m_cls) {
        tmp.clear();
        bool red;
        switch(cl.ws.getType()) {
            case CMSat::watch_binary_t:
                tmp.push_back(cl.ws.lit2());
                red = cl.ws.red();
                break;

            case CMSat::watch_tertiary_t:
                tmp.push_back(cl.ws.lit2());
                tmp.push_back(cl.ws.lit3());
                red = cl.ws.red();
                break;

            case CMSat::watch_clause_t:
                const Clause* cl_orig = solver->cl_alloc.ptr(cl.ws.get_offset());
                for(const Lit lit: *cl_orig) {
                    if (cl.lit != lit) {
                        tmp.push_back(lit);
                    }
                }
                red = cl_orig->red();
                break;
        }
        m_cls_lits.push_back(m_cls_lits_and_red(tmp, red));
    }
}

void BVA::remove_matching_clause(
    const m_cls_lits_and_red& cl_lits_and_red
    , const lit_pair lit_replace
) {
    if (solver->conf.verbosity >= 6 || bva_verbosity) {
        cout
        << "c [bva] Removing cl "
        //<< solver->watched_to_string(lit_replace, cl.ws)
        << endl;
    }

    to_remove.clear();
    to_remove.push_back(lit_replace.lit1);
    if (lit_replace.lit2 != lit_Undef) {
        to_remove.push_back(lit_replace.lit2);
    }
    for(const Lit cl_lit: cl_lits_and_red.lits) {
        to_remove.push_back(cl_lit);
    }
    touched.touch(to_remove);

    switch(to_remove.size()) {
        case 2: {
            *simplifier->limit_to_decrease -= 2*solver->watches[to_remove[0].toInt()].size();
            //bool red = !findWBin(solver->watches, to_remove[0], to_remove[1], false);
            bool red = false;
            *(solver->drup) << del << to_remove << fin;
            solver->detach_bin_clause(to_remove[0], to_remove[1], red);
            break;
        }

        case 3: {
            std::sort(to_remove.begin(), to_remove.end());
            *simplifier->limit_to_decrease -= 2*solver->watches[to_remove[0].toInt()].size();
            //bool red = !findWTri(solver->watches, to_remove[0], to_remove[1], to_remove[2], false);
            bool red = false;
            *(solver->drup) << del << to_remove << fin;
            solver->detach_tri_clause(to_remove[0], to_remove[1], to_remove[2], red);
            break;
        }

        default:
            Clause* cl_new = find_cl_for_bva(to_remove, cl_lits_and_red.red);
            simplifier->unlink_clause(solver->cl_alloc.get_offset(cl_new));
            break;
    }
}

Clause* BVA::find_cl_for_bva(
    const vector<Lit>& torem
    , const bool red
) const {
    Clause* cl = NULL;
    for(const Lit lit: torem) {
        seen[lit.toInt()] = 1;
    }
    for(Watched w: solver->watches[torem[0].toInt()]) {
        if (!w.isClause())
            continue;

        cl = solver->cl_alloc.ptr(w.get_offset());
        if (cl->red() != red
            || cl->size() != torem.size()
        ) {
            continue;
        }

        bool OK = true;
        for(const Lit lit: *cl) {
            if (seen[lit.toInt()] == 0) {
                OK = false;
                break;
            }
        }

        if (OK)
            break;
    }

    for(const Lit lit: torem) {
        seen[lit.toInt()] = 0;
    }

    assert(cl != NULL);
    return cl;
}

bool BVA::add_longer_clause(const Lit new_lit, const OccurClause& cl)
{
    vector<Lit>& lits = bva_tmp_lits;
    lits.clear();
    switch(cl.ws.getType()) {
        case CMSat::watch_binary_t: {
            lits.resize(2);
            lits[0] = new_lit;
            lits[1] = cl.ws.lit2();
            solver->add_clause_int(lits, false, ClauseStats(), false, &lits, true, new_lit);
            break;
        }

        case CMSat::watch_tertiary_t: {
            lits.resize(3);
            lits[0] = new_lit;
            lits[1] = cl.ws.lit2();
            lits[2] = cl.ws.lit3();
            solver->add_clause_int(lits, false, ClauseStats(), false, &lits, true, new_lit);
            break;
        }

        case CMSat::watch_clause_t: {
            const Clause& orig_cl = *solver->cl_alloc.ptr(cl.ws.get_offset());
            lits.resize(orig_cl.size());
            for(size_t i = 0; i < orig_cl.size(); i++) {
                if (orig_cl[i] == cl.lit) {
                    lits[i] = new_lit;
                } else {
                    lits[i] = orig_cl[i];
                }
            }
            Clause* newCl = solver->add_clause_int(lits, false, orig_cl.stats, false, &lits, true, new_lit);
            if (newCl != NULL) {
                simplifier->linkInClause(*newCl);
                ClOffset offset = solver->cl_alloc.get_offset(newCl);
                simplifier->clauses.push_back(offset);
            }
            break;
        }
    }
    touched.touch(lits);

    return solver->okay();
}

string BVA::PotentialClause::to_string(const Solver* solver) const
{
    std::stringstream ss;
    ss << solver->watched_to_string(occur_cl.lit, occur_cl.ws)
    << " -- (diff) lit: " << lits.lit1 << ", " << lits.lit2;

    return ss.str();
}

int BVA::simplification_size(
    const int m_lits_size
    , const int m_cls_size
) const {
    return m_lits_size*m_cls_size-m_lits_size-m_cls_size;
}

void BVA::fill_potential(const Lit lit)
{
    for(const OccurClause& c: m_cls) {
        if (*simplifier->limit_to_decrease < 0)
            break;

        const Lit l_min = least_occurring_except(c);
        if (l_min == lit_Undef)
            continue;

        solver->watches.prefetch(l_min.toInt());
        m_lits_this_cl = m_lits;
        *simplifier->limit_to_decrease -= m_lits_this_cl.size();
        for(const lit_pair lits: m_lits_this_cl) {
            seen2[lits.hash(seen2.size())] = 1;
        }

        if (solver->conf.verbosity >= 6 || bva_verbosity) {
            cout
            << "c [bva] Examining clause for addition to 'potential':"
            << solver->watched_to_string(c.lit, c.ws)
            << " -- Least occurring in this CL: " << l_min
            << endl;
        }

        *simplifier->limit_to_decrease -= (long)solver->watches[l_min.toInt()].size()*3;
        for(const Watched& d_ws: solver->watches[l_min.toInt()]) {
            if (*simplifier->limit_to_decrease < 0)
                goto end;

            OccurClause d(l_min, d_ws);
            const size_t sz_c = solver->cl_size(c.ws);
            const size_t sz_d = solver->cl_size(d.ws);
            if (c.ws != d.ws
                && (sz_c == sz_d
                    || (sz_c+1 == sz_d
                        && solver->conf.bva_also_twolit_diff
                        && (long)solver->sumConflicts() >= solver->conf.bva_extra_lit_and_red_start
                    )
                )
                && !solver->redundant(d.ws)
                && lit_diff_watches(c, d) == lit
            ) {
                const lit_pair diff = lit_diff_watches(d, c);
                if (seen2[diff.hash(seen2.size())] == 0) {
                    *simplifier->limit_to_decrease -= 3;
                    potential.push_back(PotentialClause(diff, c));
                    m_lits_this_cl.push_back(diff);
                    seen2[diff.hash(seen2.size())] = 1;

                    if (solver->conf.verbosity >= 6 || bva_verbosity) {
                        cout
                        << "c [bva] Added to P: "
                        << potential.back().to_string(solver)
                        << endl;
                    }
                }
            }
        }

        end:
        for(const lit_pair lits: m_lits_this_cl) {
            seen2[lits.hash(seen2.size())] = 0;
        }
    }
}


bool BVA::VarBVAOrder::operator()(const uint32_t lit1_uint, const uint32_t lit2_uint) const
{
    return watch_irred_sizes[lit1_uint] > watch_irred_sizes[lit2_uint];
}



bool BVA::simplifies_system(const size_t num_occur) const
{
    //If first run, at least 2 must match, nothing else matters
    if (m_lits.size() == 1) {
        return num_occur >= 2;
    }

    assert(m_lits.size() > 1);
    int orig_num_red = simplification_size(m_lits.size(), m_cls.size());
    int new_num_red = simplification_size(m_lits.size()+1, num_occur);

    if (new_num_red <= 0)
        return false;

    if (new_num_red < orig_num_red)
        return false;

    return true;
}



BVA::lit_pair BVA::most_occuring_lit_in_potential(size_t& largest)
{
    largest = 0;
    lit_pair most_occur = lit_pair(lit_Undef, lit_Undef);
    if (potential.size() > 1) {
        *simplifier->limit_to_decrease -= (double)potential.size()*(double)std::log(potential.size())*0.2;
        std::sort(potential.begin(), potential.end());
    }

    lit_pair last_occur = lit_pair(lit_Undef, lit_Undef);
    size_t num = 0;
    for(const PotentialClause pot: potential) {
        if (last_occur != pot.lits) {
            if (num >= largest) {
                largest = num;
                most_occur = last_occur;
            }
            last_occur = pot.lits;
            num = 1;
        } else {
            num++;
        }
    }
    if (num >= largest) {
        largest = num;
        most_occur = last_occur;
    }

    if (solver->conf.verbosity >= 5 || bva_verbosity) {
        cout
        << "c [bva] ---> Most occuring lit in p: " << most_occur.lit1 << ", " << most_occur.lit2
        << " occur num: " << largest
        << endl;
    }

    return most_occur;
}

BVA::lit_pair BVA::lit_diff_watches(const OccurClause& a, const OccurClause& b)
{
    //assert(solver->cl_size(a.ws) == solver->cl_size(b.ws));
    assert(a.lit != b.lit);
    solver->for_each_lit(b, [&](const Lit lit) {seen[lit.toInt()] = 1;}, simplifier->limit_to_decrease);

    size_t num = 0;
    lit_pair toret = lit_pair(lit_Undef, lit_Undef);
    const auto check_seen = [&] (const Lit lit) {
        if (seen[lit.toInt()] == 0) {
            if (num == 0)
                toret.lit1 = lit;
            else
                toret.lit2 = lit;

            num++;
        }
    };
    solver->for_each_lit(a, check_seen, simplifier->limit_to_decrease);
    solver->for_each_lit(b, [&](const Lit lit) {seen[lit.toInt()] = 0;}, simplifier->limit_to_decrease);

    if (num >= 1 && num <= 2)
        return toret;
    else
        return lit_Undef;
}



Lit BVA::least_occurring_except(const OccurClause& c)
{
    *simplifier->limit_to_decrease -= (long)m_lits.size();
    for(const lit_pair lits: m_lits) {
        seen[lits.lit1.toInt()] = 1;
        if (lits.lit2 != lit_Undef) {
            seen[lits.lit2.toInt()] = 1;
        }
    }

    Lit smallest = lit_Undef;
    size_t smallest_val = std::numeric_limits<size_t>::max();
    const auto check_smallest = [&] (const Lit lit) {
        //Must not be in m_lits
        if (seen[lit.toInt()] != 0)
            return;

        const size_t watch_size = solver->watches[lit.toInt()].size();
        if (watch_size < smallest_val) {
            smallest = lit;
            smallest_val = watch_size;
        }
    };
    solver->for_each_lit_except_watched(c, check_smallest, simplifier->limit_to_decrease);

    for(const lit_pair lits: m_lits) {
        seen[lits.lit1.toInt()] = 0;
        if (lits.lit2 != lit_Undef) {
            seen[lits.lit2.toInt()] = 0;
        }
    }

    return smallest;
}

void BVA::calc_watch_irred_sizes()
{
    watch_irred_sizes.clear();
    for(size_t i = 0; i < solver->nVars()*2; i++) {
        const Lit lit = Lit::toLit(i);
        const size_t irred_size = calc_watch_irred_size(lit);
        watch_irred_sizes.push_back(irred_size);
    }
}

size_t BVA::calc_watch_irred_size(const Lit lit) const
{
    size_t num = 0;
    watch_subarray_const ws = solver->watches[lit.toInt()];
    for(const Watched w: ws) {
        if (w.isBinary() || w.isTri()) {
            num += !w.red();
            continue;
        }

        assert(w.isClause());
        const Clause& cl = *solver->cl_alloc.ptr(w.get_offset());
        num += !cl.red();
    }

    return num;
}
