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
        module ClusterScheddParams
          def self.store_feature
            "BaseScheduler"
          end

          def self.gen_params(name, spool)
            {name=>"$(SCHEDD)", "#{name}.USE_PROCD"=>"False",
             "SCHEDD.#{name}.SCHEDD_NAME"=>"ha-schedd-#{name}@",
             "SCHEDD.#{name}.SPOOL"=>spool,
             "SCHEDD.#{name}.HISTORY"=>"$(SCHEDD.#{name}.SPOOL)/history",
             "SCHEDD.#{name}.SCHEDD_LOG"=>"$(LOG)/SchedLog-#{name}.log",
             "SCHEDD.#{name}.SCHEDD_ADDRESS_FILE"=>"$(LOG)/.schedd-#{name}_address",
             "SCHEDD.#{name}.SCHEDD_DAEMON_AD_FILE"=>"$(LOG)/.schedd-#{name}_classad"
            }
          end
        end

        module ClusterQueryServerParams
          def self.store_feature
            "BaseQueryServer"
          end

          def self.gen_params(name, spool)
            {name=>"$(QUERY_SERVER)",
             "QUERY_SERVER.#{name}.SPOOL"=>spool,
             "QUERY_SERVER.#{name}.HISTORY"=>"$(QUERY_SERVER.#{name}.SPOOL)/history",
             "QUERY_SERVER.#{name}.QUERY_SERVER_LOG"=>"$(LOG)/QueryServerLog-#{name}",
             "QUERY_SERVER.#{name}.QUERY_SERVER_ADDRESS_FILE"=>"$(LOG)/.query_server_address-#{name}",
             "QUERY_SERVER.#{name}.QUERY_SERVER_DAEMON_AD_FILE"=>"$(LOG)/.query_server_classad-#{name}"
            }
          end
        end

        module ClusterJobServerParams
          def self.store_feature
            "BaseJobServer"
          end

          def self.gen_params(name, spool)
            {name=>"$(JOB_SERVER)",
             "JOB_SERVER.#{name}.SPOOL"=>spool,
             "JOB_SERVER.#{name}.HISTORY"=>"$(JOB_SERVER.#{name}.SPOOL)/history",
             "JOB_SERVER.#{name}.JOB_SERVER_LOG"=>"$(LOG)/JobServerLog-#{name}",
             "JOB_SERVER.#{name}.JOB_SERVER_ADDRESS_FILE"=>"$(LOG)/.job_server_address-#{name}",
             "JOB_SERVER.#{name}.JOB_SERVER_DAEMON_AD_FILE"=>"$(LOG)/.job_server_classad-#{name}"
            }
          end
        end

        module ClusterAddOps
          def act
            if not @options.has_key?(:wallaby_only)
              # Cluster Suite config
              restarts = @options[:max_restarts] ? @options[:max_restarts] : 3
              expire = @options[:expire] ? @options[:expire] : 300

              if @options.has_key?(:new_cluster)
                `#{@ccs} --stopall`
                `#{@ccs} --createcluster "HACondorCluster" #{@password}`
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
              `#{@ccs} --addsubservice "#{@service}" condor name="#{@options[:name]}" type="#{@subsys}" #{@password}`
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

        module ClusterDeleteOps
          def act
            if @options.has_key?(:nodes)
              puts "Warning: nodes are ignored for delete operations"
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

        module ClusterNodeOps
          def act
            if not @options.has_key?(:wallaby_only)
              modify_ccs_nodes
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              modify_group_membership
            end

            activate_changes
            return 0
          end
        end

        module ClusterCommonOps
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

            if store.checkGroupValidity([group_name]) != []
              exit!(1, "No group in store for #{print_subsys} #{@name}")
            end

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
        
          def prepositions
            ["to", "from"]
          end

          def action
            (self.class.opname.split("-") - prepositions - [subsys.delete("_")])[1..-1].join('_').to_sym
          end

          def subsys
            self.class.opname.split("-").last.gsub(/(server)/, '_\1')
          end

          def print_subsys
            @subsys.split('_').collect{|n| n.capitalize}.join(" ")
          end

          def parse_args(*args)
            @action = action
            @subsys = subsys
            valid_args = cmd_args[@action]
            exit!(1, "Too few arguments") if args.length < valid_args.length

            args.each do |arg|
              pair = arg.split('=', 2)
              if pair.length > 1
                exit!(1, "Argument #{pair[0]} is not valid") if not valid_args.include?(pair[0].to_sym)
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

            @domain = "#{print_subsys} #{@options[:name]} Failover Domain"
            @password = "--password #{@options[:password]}" if @options.has_key?(:password)
            @service = "HA #{print_subsys} #{@options[:name]}"
            @name = @options[:name]
            @tag = "CLUSTER_NAMES"
          end

          def group_name
            "Automatically generated High-Availability configuration for #{print_subsys} #{@name}"
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
            `#{@ccs} --rmsubservice "#{@service}" condor #{@password}`
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
            group.modifyParams("ADD", {@tag=>">= #{@name}"}, {})
          end

          def delete_group
            store.console.objects(:class=>"Group").each {|g| store.removeGroup(g.name) if (g.params.keys.include?(@tag) && g.params[@tag].include?(@name))}
          end

          def get_const_name
            Mrg::Grid::Config::Shell.constants.grep(/^Cluster#{@subsys.split('_').collect {|n| n.capitalize}}Params/)[0].to_sym
          end

          def get_params
            Mrg::Grid::Config::Shell.const_get(get_const_name).gen_params(@name, @options[:spool])
          end

          def get_store_feature
            Mrg::Grid::Config::Shell.const_get(get_const_name).store_feature
          end

          def add_params_to_store
            # Add Parameters if they don't exist
            store.checkParameterValidity(get_params.keys).each do |pname|

            # Add missing params
              pargs = ["--needs-restart", "yes", "--kind", get_kind(pname), "--description", "Created for HA #{print_subsys} #{@name}", pname]
              Mrg::Grid::Config::Shell::AddParam.new(store, "").main(pargs)
            end
          end

          def get_kind(n)
            n.upcase.include?("USE_PROCD") ? "Boolean" : "String"
          end

          def add_params_to_group
            param_list = []
            get_params.each_pair do |n, v|
              param_list << "#{n}=#{v}"
            end
            Mrg::Grid::Config::Shell::ReplaceGroupParam.new(store, "").main([group_name] + param_list)
            Mrg::Grid::Config::Shell::ReplaceGroupFeature.new(store, "").main([group_name] + [get_store_feature])
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

        # Schedd related commands
        class ClusterCreateSchedd < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterAddOps

          def self.opname
            "cluster-create-schedd"
          end
        
          def self.description
            "Create an HA Schedd cluster configuration"
          end
        
          def supports_add
            true
          end
        end

        class ClusterDeleteSchedd < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterDeleteOps

          def self.opname
            "cluster-delete-schedd"
          end
        
          def self.description
            "Delete an HA Schedd cluster configuration"
          end
        end

        class ClusterAddNodeToSchedd < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-add-node-to-schedd"
          end
        
          def self.description
            "Add a node to an existing HA Schedd cluster configuration"
          end
        end

        class ClusterRemoveNodeFromSchedd < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-remove-node-from-schedd"
          end
        
          def self.description
            "Remove a node from an existing HA Schedd cluster configuration"
          end
        end

        class ClusterCreateJobServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterAddOps

          def self.opname
            "cluster-create-jobserver"
          end
        
          def self.description
            "Create an HA JobServer cluster configuration"
          end
        
          def supports_add
            true
          end
        end

        class ClusterDeleteJobServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterDeleteOps

          def self.opname
            "cluster-delete-jobserver"
          end
        
          def self.description
            "Delete an HA JobServer cluster configuration"
          end
        end

        class ClusterAddNodeToJobServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-add-node-to-jobserver"
          end
        
          def self.description
            "Add a node to an existing HA JobServer cluster configuration"
          end
        end

        class ClusterRemoveNodeFromJobServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-remove-node-from-jobserver"
          end
        
          def self.description
            "Remove a node from an existing HA JobServer cluster configuration"
          end
        end

        class ClusterCreateQueryServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterAddOps

          def self.opname
            "cluster-create-queryserver"
          end
        
          def self.description
            "Create an HA QueryServer cluster configuration"
          end
        
          def supports_add
            true
          end
        end

        class ClusterDeleteQueryServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterDeleteOps

          def self.opname
            "cluster-delete-queryserver"
          end
        
          def self.description
            "Delete an HA QueryServer cluster configuration"
          end
        end

        class ClusterAddNodeToQueryServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-add-node-to-queryserver"
          end
        
          def self.description
            "Add a node to an existing HA QueryServer cluster configuration"
          end
        end

        class ClusterRemoveNodeFromQueryServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-remove-node-from-queryserver"
          end
        
          def self.description
            "Remove a node from an existing HA QueryServer cluster configuration"
          end
        end

        class ClusterSyncToStore < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps

          def self.opname
            "cluster-sync-to-store"
          end
        
          def self.description
            "Replaces HA configuration(s) in the store with the cluster configuration"
          end

          def supports_options
            false
          end

          def action
            self.class.opname.split("-", 2).last.gsub(/-/, '_').to_sym
          end

          def act
            domain, spool, name, type = nil, nil, nil, nil

            # Remove all groups in the store
            remove_store_groups

            # Get current services from ccs
            services = Hash.new {|h,k| h[k] = Hash.new {|h1,k1| h1[k1] = {}}}
            `#{@ccs} --lsservices`.each do |line|
              if (not line.scan(/^service:.*/).empty?) || (not line.scan(/^resources:.*/).empty?)
                domain, spool, name, type = nil, nil, nil, nil
              end
              line.scan(/domain=([^,]+),/) {|d| domain = d[0]}
              line.scan(/mountpoint=([^,]+),/) {|m| spool = m[0]}
              line.scan(/condor:\s*name=([^,]+),\s*type=(\S+)/) {|c| name, type = c[0], c[1]}
              if domain && spool && name && type
                services[name.to_sym][:domain] = domain
                services[name.to_sym][:spool] = spool
                services[name.to_sym][:type] = type
                domain, spool, name, type = nil, nil, nil, nil
              end
            end

            # Get current failover domains from ccs
            name = ""
            domains = Hash.new {|h,k| h[k] = []}
            `#{@ccs} --lsfailoverdomain`.each do |line|
              line.scan(/^(\S.+)/) {|l| name = l[0].split(':')[0]}
              line.scan(/^\s+(.+)/) {|l| domains[name] << l[0].split(':')[0]}
            end

            services.keys.each do |n|
              @name = n.to_s
              @options[:spool] = services[n][:spool]
              @options[:nodes] = domains[services[n][:domain]]
              @subsys = services[n][:type]
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
