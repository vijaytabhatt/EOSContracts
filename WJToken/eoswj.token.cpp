#include "eoswj.token.hpp"

#include <utility>

using namespace eoswj;
using namespace eosio;

void token::create(name issuer)
{
    static constexpr int64_t inf_supply = -1;
    create_token(issuer, asset(inf_supply, SYMBOL));
}

void token::create_token(name issuer, asset maximum_supply)
{
    require_auth(_self);

    auto sym = maximum_supply.symbol;
    eosio_assert(sym.is_valid(), "invalid  token symbol name");
    eosio_assert(maximum_supply.is_valid(), "invalid supply");

    stats statstable(_self, sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    eosio_assert(existing == statstable.end(), "token with this name already exists" );

    statstable.emplace(_self, [&](auto& s) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply    = maximum_supply;
        s.issuer        = issuer;
    });
}

void token::issue(name to, asset quantity, std::string memo)
{
    auto sym = quantity.symbol;
    eosio_assert(sym.is_valid(), "invalid symbol name");
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    auto sym_code = sym.code().raw();
    stats statstable(_self, sym_code);
    auto existing = statstable.find(sym_code);
    eosio_assert(existing != statstable.end(), "Invalid  token!");
    const auto& st = *existing;

    require_auth(st.issuer);
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must issue positive quantity");
    eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

    statstable.modify(st, same_payer, [&]( auto& s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);
    if(to != st.issuer) 
    {
        SEND_INLINE_ACTION(*this, transfer, { st.issuer, "active"_n }, 
            { st.issuer, to, quantity, std::move(memo) }
        );
    }
}

void token::burn(asset quantity, std::string memo)
{
    auto sym = quantity.symbol;
    eosio_assert(sym.is_valid(), "Invalid symbol name");
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    auto sym_code = sym.code().raw();
    stats statstable(_self, sym_code);
    auto existing = statstable.find(sym_code);
    eosio_assert(existing != statstable.end(), "Invalid  token!");
    const auto& st = *existing;

    const auto& from = st.issuer;
    require_auth(from);
    eosio_assert(quantity.is_valid(), "Invalid quantity!");
    eosio_assert(quantity.amount > 0, "Must burn positive quantity!");

    eosio_assert(quantity.symbol == st.supply.symbol, "Symbol precision mismatch!");
    eosio_assert(quantity.amount <= st.supply.amount, "Quantity exceeds available supply!");

    sub_balance(from, quantity);
    statstable.modify(st, same_payer, [&](auto& s) {
        s.supply -= quantity;
    });
}

void token::open(eosio::name owner, const symbol& symbol, name payer)
{
    require_auth(payer);

    auto sym_code_raw = symbol.code().raw();
    stats statstable(_self, sym_code_raw);
    const auto& st = statstable.get(sym_code_raw, "This symbol does not exist");
    eosio_assert(st.supply.symbol == symbol, "precision mismatch");
    
    accounts acnts(_self, owner.value);
    auto it = acnts.find(sym_code_raw);
    if(it == acnts.end()) 
    {
        acnts.emplace( payer, [&]( auto& a ){
            a.balance = asset{ 0, symbol };
        });
    }
}

void token::close(name owner, const symbol& symbol)
{
    require_auth( owner );

    auto sym_code_raw = symbol.code().raw();
    stats statstable(_self, sym_code_raw);
    const auto& st = statstable.get(sym_code_raw, "symbol does not exist");
    eosio_assert(st.supply.symbol == symbol, "precision mismatch");

    accounts acnts(_self, owner.value);
    auto it = acnts.find(sym_code_raw);
    eosio_assert(it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
    eosio_assert(it->balance.amount == 0, "Cannot close because the balance is not zero." );
    acnts.erase( it );
}

void token::transfer(name from, name to, asset quantity, std::string memo)
{
    autopayer = has_auth(to) ? to : from;
    transfer_token(from, to, payer, quantity, std::move(memo));
}

void token::sub_balance(name owner, asset value)
{
    accounts from_acnts(_self, owner.value);

    const auto& from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
    eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");

    from_acnts.modify(from, owner, [&](auto& a) {
        a.balance -= value;
    });
}

void token::transfer_token(name from, name to, name payer, asset quantity, std::string memo)
{
    eosio_assert(from != to, "cannot transfer to self");
    require_auth(from);
    eosio_assert(is_account(to), "to account does not exist");

    auto sym_code = quantity.symbol.code().raw();
    stats statstable(_self, sym_code);
    const auto& st = statstable.get(sym_code);

    

    require_recipient(from);
    require_recipient(to);

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");
    eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
}


void token::add_balance(name owner, asset value, name payer)
{
    accounts to_acnts(_self, owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if(to == to_acnts.end())
    {
        to_acnts.emplace(payer, [&]( auto& a) {
            a.balance = value;
        });
    }
    else 
    {
        to_acnts.modify(to, same_payer, [&](auto& a) {
            a.balance += value;
        });
    }
}

EOSIO_DISPATCH(eoswj::token, (create)(issue)(burn)(transfer)(open)(close));
