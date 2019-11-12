#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/name.hpp>
#include <eosiolib/symbol.hpp>
#include <string>

namespace eoswj {

    using eosio::asset;
    using eosio::contract;
    using eosio::name;
    using eosio::symbol;
    using eosio::symbol_code;

    static constexpr auto WJSYMBOL = symbol("WJ", 0);

    class [[eosio::contract("eoswj.token")]] token : public contract 
    {
    public:
        using contract::contract;

    //public_api:
        [[eosio::action]]
        void open(name owner, const symbol& symbol, name payer);

        [[eosio::action]]
        void close(name owner, const symbol& symbol);

        [[eosio::action]]
        void transfer(name from, name to, asset quantity, std::string memo);
		
		
		static asset get_balance(name token_contract, name owner, symbol_code sym)
        {
            accounts accnt(token_contract, owner.value);
            const auto& ac = accnt.get(sym.raw());
            return ac.balance;
        }

        static asset get_supply(name token_contract, symbol_code sym)
        {
            stats statstable(token_contract, sym.raw());
            const auto& st = statstable.get(sym.raw());
            return st.supply;
        }

        

        static bool has_balance(name token_contract, name owner, symbol_code sym)
        {
            accounts accnt(token_contract, owner.value);
            return accnt.find(sym.raw()) != accnt.end();
        }

    //private_api:
        [[eosio::action]]
        void create(name issuer);

        [[eosio::action]]
        void issue(name to, asset quantity, std::string memo);

        [[eosio::action]]
        void burn(asset quantity, std::string memo);

    private:
        struct [[eosio::table]] account
        {
            asset    balance;
            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };

        struct [[eosio::table]] currency_stat
        {
            asset       supply;
            asset       max_supply;
            eosio::name issuer;

            uint64_t primary_key() const { return supply.symbol.code().raw(); }
        };

        typedef eosio::multi_index<"accounts"_n, account> accounts;
        typedef eosio::multi_index<"stat"_n, currency_stat> stats;

        void create_token(eosio::name issuer, asset maximum_supply);
        void transfer_token(eosio::name from, name to, name payer, asset quantity, std::string memo);

        void sub_balance(name owner, asset value);
        void add_balance(name owner, asset value, name payer);
    };
} /// namespace eoswj
