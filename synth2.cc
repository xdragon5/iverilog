/*
 * Copyright (c) 2002-2003 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: synth2.cc,v 1.42 2005/04/24 23:44:02 steve Exp $"
#endif

# include "config.h"

# include  "functor.h"
# include  "netlist.h"
# include  "netmisc.h"
# include  "compiler.h"
# include  <assert.h>


bool NetProc::synth_async(Design*des, NetScope*scope,
			  const NetBus&nex_map, NetBus&nex_out)
{
      return false;
}

bool NetProc::synth_sync(Design*des, NetScope*scope, NetFF*ff,
			 const NetBus&nex_map, NetBus&nex_out,
			 const svector<NetEvProbe*>&events)
{
      if (events.count() > 0) {
	    cerr << get_line() << ": error: Events are unaccounted"
		 << " for in process synthesis." << endl;
	    des->errors += 1;
      }

	/* Synthesize the input to the DFF. */
      return synth_async(des, scope, nex_map, nex_out);
}

static unsigned find_nexus_in_set(const NetBus&nset, const Nexus*nex)
{
      unsigned idx = 0;
      for (idx = 0 ;  idx < nset.pin_count() ;  idx += 1)
	    if (nset.pin(idx).nexus() == nex)
		  return idx;

      return idx;
}

/*
 * Async synthesis of assignments is done by synthesizing the rvalue
 * expression, then connecting the l-value directly to the output of
 * the r-value.
 *
 * The nex_map is the O-set for the statement, and lists the positions
 * of the outputs as the caller wants results linked up. The nex_out,
 * however, is the set of nexa that are to actually get linked to the
 * r-value.
 */
bool NetAssignBase::synth_async(Design*des, NetScope*scope,
				const NetBus&nex_map, NetBus&nex_out)
{
      NetNet*rsig = rval_->synthesize(des);
      assert(rsig);

      NetNet*lsig = lval_->sig();
      if (!lsig) {
	    cerr << get_line() << ": error: "
		 << "NetAssignBase::synth_async on unsupported lval ";
	    dump_lval(cerr);
	    cerr << endl;
	    return false;
      }
      assert(lval_->more == 0);

#if 0
	/* The l-value and r-value map must have the same width. */
      if (lval_->lwidth() != nex_map->vector_width()) {
	    cerr << get_line() << ": error: Assignment synthesis: "
		 << "vector width mismatch, "
	         << lval_->lwidth() << " bits != "
		 << nex_map->vector_width() << " bits." << endl;
	    return false;
      }
#else
	/* For now, assume there is exactly one output. */
      assert(nex_out.pin_count() == 1);
#endif

      connect(nex_out.pin(0), rsig->pin(0));

	/* This lval_ represents a reg that is a WIRE in the
	   synthesized results. This function signals the destructor
	   to change the REG that this l-value refers to into a
	   WIRE. It is done then, at the last minute, so that pending
	   synthesis can continue to work with it as a WIRE. */
      lval_->turn_sig_to_wire_on_release();

      return true;
}

/*
 * Sequential blocks are translated to asynchronous logic by
 * translating each statement of the block, in order, into gates. The
 * nex_out for the block is the union of the nex_out for all the
 * substatements.
 */
bool NetBlock::synth_async(Design*des, NetScope*scope,
			   const NetBus&nex_map, NetBus&nex_out)
{
      if (last_ == 0) {
	    return true;
      }

#if 0
      const perm_string tmp1 = perm_string::literal("tmp1");
      const perm_string tmp2 = perm_string::literal("tmp2");

      bool flag = true;
      NetProc*cur = last_;
      do {
	    cur = cur->next_;

	      /* Create a temporary nex_map for the substatement. */
	    NexusSet tmp_set;
	    cur->nex_output(tmp_set);
	    NetNet*tmp_map = new NetNet(scope, tmp1, NetNet::WIRE,
					tmp_set.count());
	    for (unsigned idx = 0 ;  idx < tmp_map->pin_count() ;  idx += 1)
		  connect(tmp_set[idx], tmp_map->pin(idx));

	      /* Create also a temporary net_out to collect the
		 output. */
	    NetNet*tmp_out = new NetNet(scope, tmp2, NetNet::WIRE,
					tmp_set.count());

	    bool ok_flag = cur->synth_async(des, scope, tmp_map, tmp_out);
	    flag = flag && ok_flag;

	    if (ok_flag == false)
		  continue;

	      /* Use the nex_map to link up the output from the
		 substatement to the output of the block as a whole. */
	    for (unsigned idx = 0 ;  idx < tmp_out->pin_count() ; idx += 1) {
		  unsigned ptr = find_nexus_in_set(nex_map, tmp_set[idx]);
		  connect(nex_out->pin(ptr), tmp_out->pin(idx));
	    }

	    delete tmp_map;
	    delete tmp_out;

      } while (cur != last_);

      DEBUG_SYNTH2_EXIT("NetBlock",flag)
      return flag;
#else
      cerr << get_line() << ": sorry: "
	    "forgot how to implement NetBlock::synth_async" << endl;
      return false;
#endif
}

bool NetCase::synth_async(Design*des, NetScope*scope,
			  const NetBus&nex_map, NetBus&nex_out)
{
#if 0
      unsigned cur;

      NetNet*esig = expr_->synthesize(des);

	/* Scan the select vector looking for constant bits. The
	   constant bits will be elided from the select input connect,
	   but we still need to keep track of them. */
      unsigned sel_pins = 0;
      unsigned long sel_mask = 0;
      unsigned long sel_ref = 0;
      for (unsigned idx = 0 ;  idx < esig->pin_count() ;  idx += 1) {

	    if (esig->pin(idx).nexus()->drivers_constant()) {
		  verinum::V bit = esig->pin(idx).nexus()->driven_value();
		  if (bit == verinum::V1)
			sel_ref |= 1 << idx;

	    } else {
		  sel_pins += 1;
		  sel_mask |= 1 << idx;
	    }
      }

	/* Build a map of guard values to mux select values. This
	   helps account for constant select bits that are being
	   elided. */
      map<unsigned long,unsigned long>guard2sel;
      cur = 0;
      for (unsigned idx = 0 ;  idx < (1U<<esig->vector_width()) ;  idx += 1) {
	    if ((idx & ~sel_mask) == sel_ref) {
		  guard2sel[idx] = cur;
		  cur += 1;
	    }
      }
      assert(cur == (1U << sel_pins));

      NetMux*mux = new NetMux(scope, scope->local_symbol(),
			      nex_out->pin_count(),
			      1U << sel_pins, sel_pins);

	/* Connect the non-constant select bits to the select input of
	   the mux device. */
      cur = 0;
      for (unsigned idx = 0 ;  idx < esig->pin_count() ;  idx += 1) {
	      /* skip bits that are known to be constant. */
	    if ((sel_mask & (1U << idx)) == 0)
		  continue;

	    connect(mux->pin_Sel(cur), esig->pin(idx));
	    cur += 1;
      }
      assert(cur == sel_pins);

	/* Hook up the output of the mux to the mapped output pins. */
      for (unsigned idx = 0 ;  idx < mux->width() ;  idx += 1)
	    connect(nex_out->pin(idx), mux->pin_Result(idx));

      NetProc**statement_map = new NetProc*[1 << sel_pins];
      for (unsigned item = 0 ;  item < (1U<<sel_pins) ;  item += 1)
	    statement_map[item] = 0;

	/* Assign the input statements to MUX inputs. This involves
	   calculating the guard value, passing that through the
	   guard2sel map, then saving the statement in the
	   statement_map. If I find a default case, then save that for
	   use later. */
      NetProc*default_statement = 0;
      for (unsigned item = 0 ;  item < nitems_ ;  item += 1) {
	      /* Skip the default case this pass. */
	    if (items_[item].guard == 0) {
		  default_statement = items_[item].statement;
		  continue;
	    }

	    NetEConst*ge = dynamic_cast<NetEConst*>(items_[item].guard);
	    assert(ge);
	    verinum gval = ge->value();
	    unsigned sel_idx = guard2sel[gval.as_ulong()];

	    assert(items_[item].statement);
	    statement_map[sel_idx] = items_[item].statement;
      }

	/* Now that statements match with mux inputs, synthesize the
	   sub-statements. If I get to an input that has no statement,
	   then use the default statement there. */
      NetNet*default_sig = 0;
      for (unsigned item = 0 ;  item < (1U<<sel_pins) ;  item += 1) {

	      /* Detect the case that this is a default input, and I
		 have a precalculated default_sig. */
	    if ((statement_map[item] == 0) && (default_sig != 0)) {
		for (unsigned idx = 0 ;  idx < mux->width() ;  idx += 1)
		      connect(mux->pin_Data(idx, item), default_sig->pin(idx));
		continue;
	    }

	    NetNet*sig = new NetNet(scope, scope->local_symbol(),
				    NetNet::WIRE, nex_map->pin_count());
	    sig->local_flag(true);

	    if (statement_map[item] == 0) {
		  statement_map[item] = default_statement;
		  default_statement = 0;
		  default_sig = sig;
	    }

	    if (statement_map[item] == 0) {
		  /* Missing case and no default; this could still be
		   * synthesizable with synchronous logic, but not here. */
		  DEBUG_SYNTH2_EXIT("NetCase", false)
		  return false;
	    }
	    statement_map[item]->synth_async(des, scope, nex_map, sig);

	    for (unsigned idx = 0 ;  idx < mux->width() ;  idx += 1)
		  connect(mux->pin_Data(idx, item), sig->pin(idx));
      }

      delete[]statement_map;
      des->add_node(mux);

      DEBUG_SYNTH2_EXIT("NetCase", true)
      return true;
#else
      cerr << get_line() << ": sorry: forgot how to implement "
	   << "NetCase::synth_async" << endl;
      return false;
#endif
}

bool NetCondit::synth_async(Design*des, NetScope*scope,
			    const NetBus&nex_map, NetBus&nex_out)
{
#if 0
      NetNet*ssig = expr_->synthesize(des);
      assert(ssig);

      if (if_ == 0) {
	    DEBUG_SYNTH2_EXIT("NetCondit",false)
	    return false;
      }
      if (else_ == 0) {
	    cerr << get_line() << ": error: Asynchronous if statement"
		 << " is missing the else clause." << endl;
	    DEBUG_SYNTH2_EXIT("NetCondit",false)
	    return false;
      }

      assert(if_ != 0);
      assert(else_ != 0);

      NetNet*asig = new NetNet(scope, scope->local_symbol(),
			       NetNet::WIRE, nex_map->pin_count());
      asig->local_flag(true);

      bool flag;
      flag = if_->synth_async(des, scope, nex_map, asig);
      if (!flag) {
	    delete asig;
	    DEBUG_SYNTH2_EXIT("NetCondit",false)
	    return false;
      }

      NetNet*bsig = new NetNet(scope, scope->local_symbol(),
			       NetNet::WIRE, nex_map->pin_count());
      bsig->local_flag(true);

      flag = else_->synth_async(des, scope, nex_map, bsig);
      if (!flag) {
	    delete asig;
	    delete bsig;
	    DEBUG_SYNTH2_EXIT("NetCondit",false)
	    return false;
      }

      NetMux*mux = new NetMux(scope, scope->local_symbol(),
			      nex_out->vector_width(), 2, 1);

      connect(mux->pin_Sel(),   ssig->pin(0));
      connect(mux->pin_Data(1), asig->pin(0));
      connect(mux->pin_Data(0), bsig->pin(0));
      connect(nex_out->pin(0), mux->pin_Result());

      des->add_node(mux);

      DEBUG_SYNTH2_EXIT("NetCondit",true)
      return true;

#else
      cerr << get_line() << ": sorry: "
	   << "Forgot to implement NetCondit::synth_async" << endl;
      return false;
#endif
}

bool NetEvWait::synth_async(Design*des, NetScope*scope,
			    const NetBus&nex_map, NetBus&nex_out)
{
      bool flag = statement_->synth_async(des, scope, nex_map, nex_out);
      return flag;
}

/*
 * This method is called when the process is shown to be
 * asynchronous. Figure out the nexus set of outputs from this
 * process, and pass that to the synth_async method for the statement
 * of the process. The statement will connect its output to the
 * nex_out set, using the nex_map as a guide. Starting from the top,
 * the nex_map is the same as the nex_map.
 */
bool NetProcTop::synth_async(Design*des)
{
      NexusSet nex_set;
      statement_->nex_output(nex_set);

      if (debug_synth2) {
	    cerr << get_line() << ": debug: Process has "
		 << nex_set.count() << " outputs." << endl;
      }

#if 0
      const perm_string tmp1 = perm_string::literal("tmp");
      NetNet*nex_out = new NetNet(scope(), tmp1, NetNet::WIRE,
				  nex_set.count());
      for (unsigned idx = 0 ;  idx < nex_out->pin_count() ;  idx += 1)
	    connect(nex_set[idx], nex_out->pin(idx));

      bool flag = statement_->synth_async(des, scope(), nex_out, nex_out);

      delete nex_out;
      return flag;
#else
      cerr << get_line() << ": sorry: "
	   << "forgot to implement NetProcTop::synth_async" << endl;
      return false;
#endif
}

/*
 * This method is called when a block is encountered near the surface
 * of a synchronous always statement. For example, this code will be
 * invoked for input like this:
 *
 *     always @(posedge clk...) begin
 *           <statement1>
 *           <statement2>
 *           ...
 *     end
 *
 * This needs to be split into a DFF bank for each statement, because
 * the statements may each infer different reset and enable signals.
 */
bool NetBlock::synth_sync(Design*des, NetScope*scope, NetFF*ff,
			   const NetBus&nex_map, NetBus&nex_out,
			   const svector<NetEvProbe*>&events_in)
{
      if (last_ == 0) {
	    return true;
      }

#if 0
      bool flag = true;

      const perm_string tmp1 = perm_string::literal("tmp1");
      const perm_string tmp2 = perm_string::literal("tmp2");

	/* Keep an accounting of which statement accounts for which
	   bit slice of the FF bank. This is used for error checking. */
      NetProc**pin_accounting = new NetProc* [ff->pin_count()];
      for (unsigned idx = 0 ;  idx < ff->pin_count() ;  idx += 1)
	    pin_accounting[idx] = 0;

      NetProc*cur = last_;
      do {
	    cur = cur->next_;

	      /* Create a temporary nex_map for the substatement. */
	    NexusSet tmp_set;
	    cur->nex_output(tmp_set);
	    NetNet*tmp_map = new NetNet(scope, tmp1, NetNet::WIRE,
					tmp_set.count());
	    for (unsigned idx = 0 ;  idx < tmp_map->pin_count() ;  idx += 1)
		  connect(tmp_set[idx], tmp_map->pin(idx));

	      /* NOTE: After this point, tmp_set should not be used as
		 the various functions I call do a lot of connecting,
		 and the nexa in the tmp_set may get realloced. Use
		 the tmp_map instead. */

	      /* Create also a temporary net_out to collect the
		 output. The tmp1 and tmp2 map and out sets together
		 are used to collect the outputs from the substatement
		 for the inputs of the FF bank. */
	    NetNet*tmp_out = new NetNet(scope, tmp2, NetNet::WIRE,
					tmp_map->pin_count());

	    verinum tmp_aset = ff->aset_value();
	    verinum tmp_sset = ff->sset_value();

	      /* Create a new DFF to handle this part of the begin-end
		 block. Connect this NetFF to the associated pins of
		 the existing wide NetFF device. While I'm at it, also
		 copy the aset_value bits for the new ff device. */
	    NetFF*ff2 = new NetFF(scope, scope->local_symbol(),
				  tmp_out->pin_count());
	    des->add_node(ff2);

	    verinum aset_value2 (verinum::V1, ff2->width());
	    verinum sset_value2 (verinum::V1, ff2->width());
	    for (unsigned idx = 0 ;  idx < ff2->width() ;  idx += 1) {
		  unsigned ptr = find_nexus_in_set(nex_map,
						   tmp_map->pin(idx).nexus());

		    /* Connect Data and Q bits to the new FF. */
		  connect(ff->pin_Data(ptr), ff2->pin_Data(idx));
		  connect(ff->pin_Q(ptr), ff2->pin_Q(idx));

		    /* Copy the asynch set bit to the new device. */
		  if (ptr < tmp_aset.len())
			aset_value2.set(idx, tmp_aset[ptr]);

		    /* Copy the synch set bit to the new device. */
		  if (ptr < tmp_sset.len())
			sset_value2.set(idx, tmp_sset[ptr]);

		  if (pin_accounting[ptr] != 0) {
			cerr << cur->get_line() << ": error: "
			     << "Synchronous output conflicts with "
			     << pin_accounting[ptr]->get_line()
			     << "." << endl;
			flag = false;

		  } else {
			pin_accounting[ptr] = cur;
		  }
	    }

	    if (ff->pin_Aclr().is_linked())
		  connect(ff->pin_Aclr(),  ff2->pin_Aclr());
	    if (ff->pin_Aset().is_linked())
		  connect(ff->pin_Aset(),  ff2->pin_Aset());
	    if (ff->pin_Sclr().is_linked())
		  connect(ff->pin_Sclr(),  ff2->pin_Sclr());
	    if (ff->pin_Sset().is_linked())
		  connect(ff->pin_Sset(),  ff2->pin_Sset());
	    if (ff->pin_Clock().is_linked())
		  connect(ff->pin_Clock(), ff2->pin_Clock());
	    if (ff->pin_Enable().is_linked())
		  connect(ff->pin_Enable(),ff2->pin_Enable());

	      /* Remember to store the aset value into the new FF. If
		 this leads to an Aset value of 0 (and Aclr is not
		 otherwise used) then move the Aset input to Aclr. */
	    if (tmp_aset.len() == ff->width()) {

		  if (aset_value2.is_zero()
		      && ff2->pin_Aset().is_linked()
		      && !ff2->pin_Aclr().is_linked()) {

			connect(ff2->pin_Aclr(), ff2->pin_Aset());
			ff2->pin_Aset().unlink();

		  } else {
			ff2->aset_value(aset_value2);
		  }
	    }

	      /* Now go on with the synchronous synthesis for this
		 subset of the statement. The tmp_map is the output
		 nexa that we expect, and the tmp_out is where we want
		 those outputs connected. */
	    bool ok_flag = cur->synth_sync(des, scope, ff2, tmp_map,
					   tmp_out, events_in);
	    flag = flag && ok_flag;

	    if (ok_flag == false)
		  continue;

	      /* Use the nex_map to link up the output from the
		 substatement to the output of the block as a
		 whole. It is occasionally possible to have outputs
		 beyond the input set, for example when the l-value of
		 an assignment is smaller then the r-value. */
	    for (unsigned idx = 0 ;  idx < tmp_out->pin_count() ; idx += 1) {
		  unsigned ptr = find_nexus_in_set(nex_map,
						   tmp_map->pin(idx).nexus());

		  if (ptr < nex_out->pin_count())
			connect(nex_out->pin(ptr), tmp_out->pin(idx));
	    }

	    delete tmp_map;
	    delete tmp_out;

      } while (cur != last_);

      delete[]pin_accounting;

	/* Done. The large NetFF is no longer needed, as it has been
	   taken up by the smaller NetFF devices. */
      delete ff;

      return flag;

#else
      cerr << get_line() << ": sorry: "
	   << "Forgot to implement NetBlock::synth_sync"
	   << endl;
      return false;
#endif
}

/*
 * This method handles the case where I find a conditional near the
 * surface of a synchronous thread. This conditional can be a CE or an
 * asynchronous set/reset, depending on whether the pin of the
 * expression is connected to an event, or not.
 */
bool NetCondit::synth_sync(Design*des, NetScope*scope, NetFF*ff,
			   const NetBus&nex_map, NetBus&nex_out,
			   const svector<NetEvProbe*>&events_in)
{
#if 0
	/* First try to turn the condition expression into an
	   asynchronous set/reset. If the condition expression has
	   inputs that are included in the sensitivity list, then it
	   is likely intended as an asynchronous input. */

      NexusSet*expr_input = expr_->nex_input();
      assert(expr_input);
      for (unsigned idx = 0 ;  idx < events_in.count() ;  idx += 1) {

	    NetEvProbe*ev = events_in[idx];
	    NexusSet pin_set;
	    pin_set.add(ev->pin(0).nexus());

	    if (! expr_input->contains(pin_set))
		  continue;

	      /* Ah, this edge is in the sensitivity list for the
		 expression, so we have an asynchronous
		 input. Synthesize the set/reset input expression. */

	    NetNet*rst = expr_->synthesize(des);
	    assert(rst->pin_count() == 1);

	      /* XXXX I really should find a way to check that the
		 edge used on the reset input is correct. This would
		 involve interpreting the exression that is fed by the
		 reset expression. */
	      //assert(ev->edge() == NetEvProbe::POSEDGE);

	      /* Synthesize the true clause to figure out what
		 kind of set/reset we have. */
	    NetNet*asig = new NetNet(scope, scope->local_symbol(),
				     NetNet::WIRE, nex_map->pin_count());
	    asig->local_flag(true);

	    assert(if_ != 0);
	    bool flag = if_->synth_async(des, scope, nex_map, asig);

	    assert(asig->pin_count() == ff->width());

	      /* Collect the set/reset value into a verinum. If
		 this turns out to be entirely 0 values, then
		 use the Aclr input. Otherwise, use the Aset
		 input and save the set value. */
	    verinum tmp (verinum::V0, ff->width());
	    for (unsigned bit = 0 ;  bit < ff->width() ;  bit += 1) {

		  assert(asig->pin(bit).nexus()->drivers_constant());
		  tmp.set(bit, asig->pin(bit).nexus()->driven_value());
	    }

	    assert(tmp.is_defined());
	    if (tmp.is_zero()) {
		  connect(ff->pin_Aclr(), rst->pin(0));

	    } else {
		  connect(ff->pin_Aset(), rst->pin(0));
		  ff->aset_value(tmp);
	    }

	    delete asig;
	    delete expr_input;

	    assert(events_in.count() == 1);
	    assert(else_ != 0);
	    flag = else_->synth_sync(des, scope, ff, nex_map,
				     nex_out, svector<NetEvProbe*>(0))
		  && flag;
            DEBUG_SYNTH2_EXIT("NetCondit",flag)
	    return flag;
      }

      delete expr_input;

	/* Detect the case that this is a *synchronous* set/reset. It
	   is not asyncronous because we know the condition is not
	   included in the sensitivity list, but if the if_ case is
	   constant (has no inputs) then we can model this as a
	   synchronous set/reset.

	   This is only synchronous set/reset if there is a true and a
	   false clause, and no inputs. The "no inputs" requirement is
	   met if the assignments are of all constant values. */
      assert(if_ != 0);
      NexusSet*a_set = if_->nex_input();

      if ((a_set->count() == 0) && if_ && else_) {

	    NetNet*rst = expr_->synthesize(des);
	    assert(rst->pin_count() == 1);

	      /* Synthesize the true clause to figure out what
		 kind of set/reset we have. */
	    NetNet*asig = new NetNet(scope, scope->local_symbol(),
				     NetNet::WIRE, nex_map->pin_count());
	    asig->local_flag(true);
	    bool flag = if_->synth_async(des, scope, nex_map, asig);

	    if (!flag) {
		  /* This path leads nowhere */
		  delete asig;
	    } else {
		  assert(asig->pin_count() == ff->width());

		    /* Collect the set/reset value into a verinum. If
		       this turns out to be entirely 0 values, then
		       use the Sclr input. Otherwise, use the Aset
		       input and save the set value. */
		  verinum tmp (verinum::V0, ff->width());
		  for (unsigned bit = 0 ;  bit < ff->width() ;  bit += 1) {

			assert(asig->pin(bit).nexus()->drivers_constant());
			tmp.set(bit, asig->pin(bit).nexus()->driven_value());
		  }

		  assert(tmp.is_defined());
		  if (tmp.is_zero()) {
			connect(ff->pin_Sclr(), rst->pin(0));

		  } else {
			connect(ff->pin_Sset(), rst->pin(0));
			ff->sset_value(tmp);
		  }

		  delete a_set;

		  assert(else_ != 0);
		  flag = else_->synth_sync(des, scope, ff, nex_map,
					   nex_out, svector<NetEvProbe*>(0))
			&& flag;
		  DEBUG_SYNTH2_EXIT("NetCondit",flag)
		  return flag;
	    }
      }

      delete a_set;

	/* Failed to find an asynchronous set/reset, so any events
	   input are probably in error. */
      if (events_in.count() > 0) {
	    cerr << get_line() << ": error: Events are unaccounted"
		 << " for in process synthesis." << endl;
	    des->errors += 1;
      }


	/* If this is an if/then/else, then it is likely a
	   combinational if, and I should synthesize it that way. */
      if (if_ && else_) {
	    bool flag = synth_async(des, scope, nex_map, nex_out);
	    DEBUG_SYNTH2_EXIT("NetCondit",flag)
	    return flag;
      }

      assert(if_);
      assert(!else_);

	/* Synthesize the enable expression. */
      NetNet*ce = expr_->synthesize(des);
      assert(ce->pin_count() == 1);

	/* What's left, is a synchronous CE statement like this:

	     if (expr_) <true statement>;

	   The expr_ expression has already been synthesized to the ce
	   net, so we connect it here to the FF. What's left is to
	   synthesize the substatement as a combinational
	   statement.

	   Watch out for the special case that there is already a CE
	   connected to this FF. This can be caused by code like this:

	     if (a) if (b) <statement>;

	   In this case, we are working on the inner IF, so we AND the
	   a and b expressions to make a new CE. */

      if (ff->pin_Enable().is_linked()) {
	    NetLogic*ce_and = new NetLogic(scope,
					   scope->local_symbol(), 3,
					   NetLogic::AND, 1);
	    des->add_node(ce_and);
	    connect(ff->pin_Enable(), ce_and->pin(1));
	    connect(ce->pin(0), ce_and->pin(2));

	    ff->pin_Enable().unlink();
	    connect(ff->pin_Enable(), ce_and->pin(0));

	    NetNet*tmp = new NetNet(scope, scope->local_symbol(),
				    NetNet::IMPLICIT, 1);
	    tmp->local_flag(true);
	    connect(ff->pin_Enable(), tmp->pin(0));

      } else {

	    connect(ff->pin_Enable(), ce->pin(0));
      }

      bool flag = if_->synth_sync(des, scope, ff, nex_map, nex_out, events_in);

      return flag;

#else
      cerr << get_line() << ": sorry: "
	   << "Forgot to implement NetCondit::synth_sync" << endl;
      return false;
#endif
}

bool NetEvWait::synth_sync(Design*des, NetScope*scope, NetFF*ff,
			   const NetBus&nex_map, NetBus&nex_out,
			   const svector<NetEvProbe*>&events_in)
{
      if (events_in.count() > 0) {
	    cerr << get_line() << ": error: Events are unaccounted"
		 << " for in process synthesis." << endl;
	    des->errors += 1;
      }

      assert(events_in.count() == 0);

	/* This can't be other then one unless there are named events,
	   which I cannot synthesize. */
      assert(nevents_ == 1);
      NetEvent*ev = events_[0];

      assert(ev->nprobe() >= 1);
      svector<NetEvProbe*>events (ev->nprobe() - 1);

	/* Get the input set from the substatement. This will be used
	   to figure out which of the probes is the clock. */
      NexusSet*statement_input = statement_ -> nex_input();

	/* Search for a clock input. The clock input is the edge event
	   that is not also an input to the substatement. */
      NetEvProbe*pclk = 0;
      unsigned event_idx = 0;
      for (unsigned idx = 0 ;  idx < ev->nprobe() ;  idx += 1) {
	    NetEvProbe*tmp = ev->probe(idx);
	    assert(tmp->pin_count() == 1);

	    NexusSet tmp_nex;
	    tmp_nex .add( tmp->pin(0).nexus() );

	    if (! statement_input ->contains(tmp_nex)) {
		  if (pclk != 0) {
			cerr << get_line() << ": error: Too many "
			     << "clocks for synchronous logic." << endl;
			cerr << get_line() << ":      : Perhaps an"
			     << " asynchronous set/reset is misused?" << endl;
			des->errors += 1;
		  }
		  pclk = tmp;

	    } else {
		  events[event_idx++] = tmp;
	    }
      }

      if (pclk == 0) {
	    cerr << get_line() << ": error: None of the edges"
		 << " are valid clock inputs." << endl;
	    cerr << get_line() << ":      : Perhaps the clock"
		 << " is read by a statement or expression?" << endl;
	    return false;
      }

      connect(ff->pin_Clock(), pclk->pin(0));
      if (pclk->edge() == NetEvProbe::NEGEDGE) {
	    perm_string polarity = perm_string::literal("Clock:LPM_Polarity");
	    ff->attribute(polarity, verinum("INVERT"));

	    if (debug_synth2) {
		  cerr << get_line() << ": debug: "
		       << "Detected a NEGEDGE clock for the synthesized ff."
		       << endl;
	    }
      }

	/* Synthesize the input to the DFF. */
      bool flag = statement_->synth_sync(des, scope, ff,
					 nex_map, nex_out, events);

      return flag;
}

/*
 * This method is called for a process that is determined to be
 * synchronous. Create a NetFF device to hold the output from the
 * statement, and synthesize that statement in place.
 */
bool NetProcTop::synth_sync(Design*des)
{
      if (debug_synth2) {
	    cerr << get_line() << ": debug: "
		 << "Process is apparently synchronous. Making NetFFs."
		 << endl;
      }

      NexusSet nex_set;
      statement_->nex_output(nex_set);

	/* Make a model FF that will connect to the first item in the
	   set, and will also take the initial connection of clocks
	   and resets. */
      NetFF*ff = new NetFF(scope(), scope()->local_symbol(),
			   nex_set[0]->vector_width());
      des->add_node(ff);
      ff->attribute(perm_string::literal("LPM_FFType"), verinum("DFF"));

      NetBus nex_d (scope(), nex_set.count());
      NetBus nex_q (scope(), nex_set.count());

	/* The Q of the NetFF devices is connected to the output that
	   we are. The nex_q is a bundle of the outputs. We will also
	   pass the nex_q as a map to the statement's synth_sync
	   method to map it to the correct nex_d pin. */
      for (unsigned idx = 0 ;  idx < nex_set.count() ;  idx += 1) {
	    connect(nex_set[idx], nex_q.pin(idx));
      }

	// Connect the input later.

	/* Synthesize the input to the DFF. */
      bool flag = statement_->synth_sync(des, scope(), ff,
					 nex_q, nex_d,
					 svector<NetEvProbe*>());
      if (! flag) {
	    delete ff;
	    return false;
      }


      NetNet*tmp = nex_d.pin(0).nexus()->pick_any_net();
      assert(tmp);

      tmp = crop_to_width(des, tmp, ff->width());
      connect(tmp->pin(0), ff->pin_Data());
      connect(nex_q.pin(0), ff->pin_Q());

      for (unsigned idx = 1 ;  idx < nex_set.count() ;  idx += 1) {
	    NetFF*ff2 = new NetFF(scope(), scope()->local_symbol(),
				  nex_set[idx]->vector_width());
	    des->add_node(ff2);

	    tmp = nex_d.pin(idx).nexus()->pick_any_net();
	    assert(tmp);

	    tmp = crop_to_width(des, tmp, ff2->width());

	    connect(nex_q.pin(idx), ff2->pin_Q());
	    connect(tmp->pin(0),    ff2->pin_Data());

	    connect(ff->pin_Clock(), ff2->pin_Clock());
	    if (ff->pin_Enable().is_linked())
		  connect(ff->pin_Enable(), ff2->pin_Enable());
	    if (ff->pin_Aset().is_linked())
		  connect(ff->pin_Aset(), ff2->pin_Aset());
	    if (ff->pin_Aclr().is_linked())
		  connect(ff->pin_Aclr(), ff2->pin_Aclr());
	    if (ff->pin_Sset().is_linked())
		  connect(ff->pin_Sset(), ff2->pin_Sset());
	    if (ff->pin_Sclr().is_linked())
		  connect(ff->pin_Sclr(), ff2->pin_Sclr());
      }

      return true;
}

class synth2_f  : public functor_t {

    public:
      void process(class Design*, class NetProcTop*);

    private:
};


/*
 * Look at a process. If it is asynchronous, then synthesize it as an
 * asynchronous process and delete the process itself for its gates.
 */
void synth2_f::process(class Design*des, class NetProcTop*top)
{
      if (top->attribute(perm_string::literal("ivl_synthesis_off")).as_ulong() != 0)
	    return;

	/* If the scope that contains this process as a cell attribute
	   attached to it, then skip synthesis. */
      if (top->scope()->attribute(perm_string::literal("ivl_synthesis_cell")).len() > 0)
	    return;

      if (top->is_synchronous()) do {
	    bool flag = top->synth_sync(des);
	    if (! flag) {
		  cerr << top->get_line() << ": error: "
		       << "Unable to synthesize synchronous process." << endl;
		  des->errors += 1;
		  return;
	    }
	    des->delete_process(top);
	    return;
      } while (0);

      if (! top->is_asynchronous()) {
	    bool synth_error_flag = false;
	    if (top->attribute(perm_string::literal("ivl_combinational")).as_ulong() != 0) {
		  cerr << top->get_line() << ": error: "
		       << "Process is marked combinational,"
		       << " but isn't really." << endl;
		  des->errors += 1;
		  synth_error_flag = true;
	    }

	    if (top->attribute(perm_string::literal("ivl_synthesis_on")).as_ulong() != 0) {
		  cerr << top->get_line() << ": error: "
		       << "Process is marked for synthesis,"
		       << " but I can't do it." << endl;
		  des->errors += 1;
		  synth_error_flag = true;
	    }

	    if (! synth_error_flag)
		  cerr << top->get_line() << ": warning: "
		       << "Process not synthesized." << endl;

	    return;
      }

      if (! top->synth_async(des)) {
	    cerr << top->get_line() << ": internal error: "
		 << "is_asynchronous does not match "
		 << "sync_async results." << endl;
	    return;
      }

      des->delete_process(top);
}

void synth2(Design*des)
{
      synth2_f synth_obj;
      des->functor(&synth_obj);
}

/*
 * $Log: synth2.cc,v $
 * Revision 1.42  2005/04/24 23:44:02  steve
 *  Update DFF support to new data flow.
 *
 */

