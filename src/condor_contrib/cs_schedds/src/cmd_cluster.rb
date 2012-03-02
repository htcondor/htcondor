# cmd_cs_control.rb: Cluster Suite control
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

module Mrg
  module Grid
    module Config
      module Shell
        module ClusterOps
          def cmd_args
            {:create=>[:name, :spool, :server, :export, :nodes],
             :delete=>[:name],
             :add_node=>[:name, :nodes],
             :remove_node=>[:name, :nodes],
             :sync_to_store=>[]}
          end

          def convert(sym)
            sym.to_s.gsub(/(.+)s$/,'\1').upcase
          end

          def modify_group_membership
            prefix = @action.to_s.include?("remove") ? "RemoveNode" : "AddNode"

            if not prefix.include?("Remove")
              store.checkNodeValidity(@options[:nodes]).each do |node|
                # Add missing node
                store.addNode(node)
              end
            end

            Mrg::Grid::Config::Shell.const_get("#{prefix}Group").new(store, "").main([group_name] + @options[:nodes])
          end

          def activate_changes
            # Activate changes in the store
            Mrg::Grid::Config::Shell::Activate.new(store, "").main([])

            # Activate cluster changes
            `#{@ccs} --sync --activate`
            `#{@ccs} --startall`
          end

          def args_for_help(list)
            list.collect{|n| n != :nodes ? "#{convert(n)}=VALUE" : "#{convert(n)} [...]"}.join(" ")
          end

          def supports_add
            false
          end

          def supports_options
            true
          end

          def init_option_parser
            @options = {}
            OptionParser.new do |opts|
              opts.banner = "Usage: #{self.class.opname} #{args_for_help(cmd_args[action])}\n"
        
              opts.on("-h", "--help", "displays this message") do
                puts @oparser
                exit
              end

              if supports_options
                opts.on("--riccipassword PASS", "The ricci user password") do |p|
                  @options[:password] = p
                end

                opts.on("-n", "--no-store", "Only configure the cluster, don't update the store") do
                  @options[:cluster_only] = true
                end

                opts.on("-s", "--store-only", "Don't configure the cluster, just update the store") do
                  @options[:wallaby_only] = true
                end

                add_options(opts) if supports_add
              end
            end
          end

          def add_options(opts)
            opts.on("-e", "--expire NUM", "Length of time to forget a restart") do |num|
              @options[:expire] = num
            end

            opts.on("-m", "--max-restarts NUM", "Maximum number of restarts before relocation (default 3)") do |num|
              @options[:max_restarts] = num
            end

            opts.on("--new-cluster", "Create a new cluster even it one already exists") do
              @options[:new_cluster] = true
            end
          end
        
          def action
            self.class.opname.split("-", 2)[1].gsub(/-/, '_').to_sym
          end

          def parse_args(*args)
            @action = action
            valid_args = cmd_args[@action]
            exit!(1, "Too few arguments") if args.length < valid_args.length

            args.each do |arg|
              pair = arg.split('=', 2)
              if pair.length > 1
                exit!(1, "Argument #{pair[0]} is not valid for action #{@action}") if not valid_args.include?(pair[0].to_sym)
                @options[pair[0].to_sym] = pair[1]
              else
                @options.has_key?(:nodes) ? @options[:nodes].push(pair[0]) : @options[:nodes] = [pair[0]]
              end
            end

            valid_args.each do |arg|
              exit!(1, "No #{arg} provided") if not @options.has_key?(arg)
            end

            # Determine if -i is needed
            `ccs -h localhost -i`
            ignore = "-i" if $?.exitstatus == 0
            @ccs = "ccs #{ignore} -h localhost"

            @domain = "Schedd #{@options[:name]} Failover Domain"
            @password = "--password #{@options[:password]}" if @options.has_key?(:password)
            @service = "HA Schedd #{@options[:name]}"
            @subsys = @options[:name]
            @group_prefix = "Automatically generated High-Availability Scheduler configuration for "
            @tag = "CLUSTER_NAMES"
          end

          def group_name
            "#{@group_prefix}#{@subsys}"
          end

          def params
            {@subsys=>"$(SCHEDD)", "#{@subsys}.USE_PROCD"=>"False",
             "SCHEDD.#{@subsys}.SCHEDD_NAME"=>"ha-schedd-#{@subsys}@",
             "SCHEDD.#{@subsys}.SPOOL"=>@options[:spool],
             "SCHEDD.#{@subsys}.HISTORY"=>"$(SCHEDD.#{@subsys}.SPOOL)/history",
             "SCHEDD.#{@subsys}.SCHEDD_LOG"=>"$(LOG)/SchedLog-#{@subsys}.log",
             "SCHEDD.#{@subsys}.SCHEDD_ADDRESS_FILE"=>"$(LOG)/.schedd-#{@subsys}-address"
            }
          end

          def self.included(receiver)
            if receiver.respond_to?(:register_callback)
              receiver.register_callback :after_option_parsing, :parse_args
            end
          end

          def modify_ccs_nodes
            cmd = (@action.to_s.include?("remove") ? "rm" : "add")
            @options[:nodes].each do |node|
              `#{@ccs} --#{cmd}node #{node} #{@password}` if cmd == "add"
              `#{@ccs} --#{cmd}failoverdomainnode '#{@domain}' #{node} #{@password}`
            end
          end

          def remove_ccs_entries
            `#{@ccs} --rmsubservice "#{@service}" condor_schedd #{@password}`
            `#{@ccs} --rmsubservice "#{@service}" netfs #{@password}`
            `#{@ccs} --rmservice "#{@service}" #{@password}`
            `#{@ccs} --rmfailoverdomain "#{@domain}" #{@password}`
          end

         def remove_store_groups
           store.console.objects(:class=>"Group").each {|g| store.removeGroup(g.name) if g.params.keys.include?(@tag)}
         end

         def add_group_to_store
           # Add the Group if it doesn't exist
           if store.checkGroupValidity([group_name]) == [group_name]
             store.addExplicitGroup(group_name)
           end
         end

         def mark_group
           if store.checkParameterValidity([@tag]) == [@tag]
             store.addParam(@tag)
           end

           add_group_to_store
           group = store.getGroupByName(group_name)
           group.modifyParams("ADD", {@tag=>">= #{@subsys}"}, {})
         end

         def delete_group
           store.console.objects(:class=>"Group").each {|g| store.removeGroup(g.name) if (g.params.keys.include?(@tag) && g.params[@tag].include?(@subsys))}
         end

         def add_params_to_store
           # Add Parameters if they don't exist
           store.checkParameterValidity(params.keys).each do |pname|
           # Add missing params
             pargs = ["--needs-restart", "yes", "--kind", get_kind(pname), "--description", "Created for HA Schedd #{@subsys}", pname]
             Mrg::Grid::Config::Shell::AddParam.new(store, "").main(pargs)
           end
         end

         def get_kind(n)
           n.upcase.include?("USE_PROCD") ? "Boolean" : "String"
         end

         def add_params_to_group
           param_list = []
           params.each_pair do |n, v|
             param_list << "#{n}=#{v}"
           end
           Mrg::Grid::Config::Shell::ReplaceGroupParam.new(store, "").main([group_name] + param_list)
           mark_group
         end

         def update_store
           # Add groups to the store
           add_group_to_store

           # Add params to the store
           add_params_to_store

           # Add params to the group
           add_params_to_group

           # Add nodes to the group
           modify_group_membership
         end
        end

        class ClusterCreate < ::Mrg::Grid::Config::Shell::Command
          include ClusterOps

          def self.opname
            "cluster-create"
          end
        
          def self.description
            "Create an HA Schedd cluster configuration"
          end
        
          def supports_add
            true
          end
        
          def act
            if not @options.has_key?(:wallaby_only)
              # Cluster Suite config
              restarts = @options[:max_restarts] ? @options[:max_restarts] : 3
              expire = @options[:expire] ? @options[:expire] : 300

              if @options.has_key?(:new_cluster)
                `#{@ccs} --stopall` if @options.has_key?(:new_cluster)
                `#{@ccs} --createcluster "HA Schedds" #{@password}`
                if $?.exitstatus != 0
                  puts "Failed to create the cluster.  Try supplying a ricci password with --riccipassword"
                  exit!(1)
                end
              end

              # Handle nodes
              `#{@ccs} --addfailoverdomain "#{@domain}" restricted #{@password}`
              exit!(1, "Failed to add cluster failover domain.  Does it already exist?") if $?.exitstatus != 0
              modify_ccs_nodes

              # Handle service
              `#{@ccs} --addservice "#{@service}" domain="#{@domain}" autostart=1 recovery=restart max_restarts=#{restarts} restart_expire_time=#{expire} #{@password}`
              exit!(1, "Failed to add cluster service.  Does it already exist?") if $?.exitstatus != 0

              # Handle subservices
              `#{@ccs} --addsubservice "#{@service}" netfs name="Job Queue for #{@options[:name]}" mountpoint="#{@options[:spool]}" host="#{@options[:server]}" export="#{@options[:export]}" options="rw,soft" force_unmount="on" #{@password}`
              `#{@ccs} --addsubservice "#{@service}" condor_schedd name="#{@options[:name]}" #{@password}`
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              # If --new-cluster is used, remove all existing groups
              remove_store_groups if @options.has_key?(:new_cluster)

              update_store
            end

            activate_changes
            return 0
          end
        end

        class ClusterDelete < ::Mrg::Grid::Config::Shell::Command
          include ClusterOps

          def self.opname
            "cluster-delete"
          end
        
          def self.description
            "Delete an HA Schedd cluster configuration"
          end
        
          def act
            if @options.has_key?(:nodes)
              puts "Warning: nodes are ignored for removal operations"
            end

            if not @options.has_key?(:wallaby_only)
              # Cluster Suite config
              remove_ccs_entries
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              # Remove the Group if it exists
              delete_group
            end

            activate_changes
            return 0
          end
        end

        class ClusterAddNode < ::Mrg::Grid::Config::Shell::Command
          include ClusterOps

          def self.opname
            "cluster-add-node"
          end
        
          def self.description
            "Add a node to an existing HA Schedd cluster configuration"
          end
        
          def act
            if not @options.has_key?(:wallaby_only)
              modify_ccs_nodes
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              mark_group
              modify_group_membership
            end

            activate_changes
            return 0
          end
        end

        class ClusterRemoveNode < ::Mrg::Grid::Config::Shell::Command
          include ClusterOps

          def self.opname
            "cluster-remove-node"
          end
        
          def self.description
            "Remove a node from an existing HA Schedd cluster configuration"
          end

          def act
            if not @options.has_key?(:wallaby_only)
              modify_ccs_nodes
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              if store.checkGroupValidity([group_name]) == []
                modify_group_membership
              end
            end

            activate_changes
            return 0
          end
        end

        class ClusterSyncToStore < ::Mrg::Grid::Config::Shell::Command
          include ClusterOps

          def self.opname
            "cluster-sync-to-store"
          end
        
          def self.description
            "Replaces HA Schedd configuration(s) in the store with the cluster configuration"
          end

          def supports_options
            false
          end

          def act
            domain, spool, subsys = nil, nil, nil

            # Remove all groups in the store
            remove_store_groups

            # Get current services from ccs
            services = Hash.new {|h,k| h[k] = Hash.new {|h1,k1| h1[k1] = {}}}
            `#{@ccs} --lsservices`.each do |line|
              if (not line.scan(/^service:.*/).empty?) || (not line.scan(/^resources:.*/).empty?)
                domain, spool, subsys = nil, nil, nil
              end
              line.scan(/domain=([^,]+),/) {|d| domain = d[0]}
              line.scan(/mountpoint=([^,]+),/) {|m| spool = m[0]}
              line.scan(/condor_schedd:\s*name=(.+)/) {|n| subsys = n[0]}
              if domain && spool && subsys
                services[subsys.to_sym][:domain] = domain
                services[subsys.to_sym][:spool] = spool
                domain, spool, subsys = nil, nil, nil
              end
            end

            # Get current failover domains from ccs
            name = ""
            domains = Hash.new {|h,k| h[k] = []}
            `#{@ccs} --lsfailoverdomain`.each do |line|
              line.scan(/^(\S.+)/) {|l| name = l[0].split(':')[0]}
              line.scan(/^\s+(.+)/) {|l| domains[name] << l[0].split(':')[0]}
            end

            services.keys.each do |sub|
              @subsys = sub.to_s
              @options[:spool] = services[sub][:spool]
              @options[:nodes] = domains[services[sub][:domain]]
              update_store
            end

            activate_changes
            return 0
          end
        end
      end
    end
  end
end
