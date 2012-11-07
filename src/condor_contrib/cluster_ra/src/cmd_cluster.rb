# cmd_cs_control.rb: Cluster control
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

require 'timeout'

module Mrg
  module Grid
    module Config
      module Shell
        module ClusterScheddParams
          def self.store_feature
            "BaseScheduler"
          end

          def self.gen_params(name, spool, sname)
            {name=>"$(SCHEDD)",
             "SCHEDD.#{name}.SCHEDD_NAME"=>"ha-schedd-#{name}@",
             "SCHEDD.#{name}.SPOOL"=>spool,
             "SCHEDD.#{name}.HISTORY"=>"$(SCHEDD.#{name}.SPOOL)/history",
             "SCHEDD.#{name}.SCHEDD_LOG"=>"$(SCHEDD_LOG)-#{name}",
             "SCHEDD.#{name}.SCHEDD_ADDRESS_FILE"=>"$(LOG)/.schedd-#{name}_address",
             "SCHEDD.#{name}.SCHEDD_DAEMON_AD_FILE"=>"$(LOG)/.schedd-#{name}_classad",
             "SCHEDD.#{name}.USE_PROCD"=>"TRUE",
             "#{name}_ENVIRONMENT"=>"_CONDOR_PROCD_ADDRESS=$(PROCD_ADDRESS).#{name} _CONDOR_SPOOL=$(SCHEDD.#{name}.SPOOL) _CONDOR_SHADOW_LOG=$(SHADOW_LOG)-#{name} _CONDOR_SHADOW_LOCK=$(SHADOW_LOCK)-#{name}",
             "SCHEDD.#{name}.RESTART_PROCD_ON_ERROR"=>"TRUE"
            }
          end
        end

        module ClusterQueryServerParams
          def self.store_feature
            "BaseQueryServer"
          end

          def self.gen_params(name, spool, dname)
            qs_name = (dname && (not dname.empty?) ? dname : "#{name}_query_server")
            {qs_name=>"$(QUERY_SERVER)",
             "QUERY_SERVER.#{qs_name}.SCHEDD_NAME"=>"ha-schedd-#{name}@",
             "QUERY_SERVER.#{qs_name}.SPOOL"=>"$(SCHEDD.#{name}.SPOOL)",
             "QUERY_SERVER.#{qs_name}.HISTORY"=>"$(QUERY_SERVER.#{qs_name}.SPOOL)/history",
             "QUERY_SERVER.#{qs_name}.QUERY_SERVER_LOG"=>"$(LOG)/QueryServerLog-#{qs_name}",
             "QUERY_SERVER.#{qs_name}.QUERY_SERVER_ADDRESS_FILE"=>"$(LOG)/.query_server_address-#{qs_name}",
             "QUERY_SERVER.#{qs_name}.QUERY_SERVER_DAEMON_AD_FILE"=>"$(LOG)/.query_server_classad-#{qs_name}",
             "QUERY_SERVER.#{qs_name}.AVIARY_PUBLISH_INTERVAL"=>"10",
             "QUERY_SERVER.#{qs_name}.AVIARY_PUBLISH_LOCATION"=>"True"
            }
          end
        end

        module ClusterJobServerParams
          def self.store_feature
            "BaseJobServer"
          end

          def self.gen_params(name, spool, dname)
            js_name = (dname && (not dname.empty?) ? dname : "#{name}_job_server")
            {js_name=>"$(JOB_SERVER)",
             "JOB_SERVER.#{js_name}.SCHEDD_NAME"=>"ha-schedd-#{name}@",
             "JOB_SERVER.#{js_name}.SPOOL"=>"$(SCHEDD.#{name}.SPOOL)",
             "JOB_SERVER.#{js_name}.HISTORY"=>"$(JOB_SERVER.#{js_name}.SPOOL)/history",
             "JOB_SERVER.#{js_name}.JOB_SERVER_LOG"=>"$(LOG)/JobServerLog-#{js_name}",
             "JOB_SERVER.#{js_name}.JOB_SERVER_ADDRESS_FILE"=>"$(LOG)/.job_server_address-#{js_name}",
             "JOB_SERVER.#{js_name}.JOB_SERVER_DAEMON_AD_FILE"=>"$(LOG)/.job_server_classad-#{js_name}"
            }
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

        module ClusterServerOps
          def act
            if not @options.has_key?(:wallaby_only)
              num = -1
              success = true
              services[@name][:condor].each_index {|loc| num = loc if services[@name][:condor][loc].type == @subsys} if services.has_key?(@name)
              sub = @subsys.to_s.split('_').collect {|n| n.capitalize}.join
              act = @action.to_s.split('_').shift
              if @action.to_s.include?("remove")
                success = exec_ccs("--rmsubservice '#{@service}' netfs:condor[#{num}]").success
              else
                exit!(1, "There is already a #{sub} configured for the HA schedd #{@name} service") if num != -1
                success = exec_ccs("--addsubservice '#{@service}' netfs:condor name='#{@options[:name]}_#{@subsys}' type='#{@subsys}' __independent_subtree='1'").success
              end
              exit!(1, "Failed to #{act} #{sub} #{preposition} the HA schedd #{@name} service") if not success
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              add_params_to_store if (not @action.to_s.include?("remove"))
              modify_params_on_group
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
             :add_jobserver=>[:name],
             :remove_jobserver=>[:name],
             :add_queryserver=>[:name],
             :remove_queryserver=>[:name],
             :sync_to_store=>[]}
          end

          def convert(sym)
            sym.to_s.gsub(/(.+)s$/,'\1').upcase
          end

          def preposition
            @action.to_s.include?("remove") ? "from" : "to"
          end

          def modify_group_membership
            prefix = @action.to_s.include?("remove") ? "RemoveNode" : "AddNode"

            if store.checkGroupValidity([group_name]) != []
              exit!(1, "No group in store for Schedd #{@name}")
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
            Mrg::Grid::Config::Shell::Activate.new(store, "").main([]) if not (@options.has_key?(:cluster_only))

            # Activate cluster changes
            if not @options.has_key?(:wallaby_only)
              exit!(1, "Failed to synchronize cluster configuration") if not exec_ccs("--sync --activate").success
              exit!(1, "Failed to start cluster services") if not exec_ccs("--startall").success
            end
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

              opts.on("--riccipassword PASS", "the ricci user password") do |p|
                @options[:password] = p
              end

              opts.on("-t", "--timeout NUM", Integer, "number of seconds to wait for a single configuration command to complete (default: 60)") do |t|
                @options[:timeout] = t
              end

              if supports_options
                opts.on("-n", "--no-store", "only configure the cluster, don't update the store") do
                  @options[:cluster_only] = true
                end

                opts.on("-s", "--store-only", "don't configure the cluster, just update the store") do
                  @options[:wallaby_only] = true
                end

                add_options(opts) if supports_add
              end
            end
          end

          def add_options(opts)
            opts.on("-e", "--expire NUM", Integer, "length of time in seconds to forget a restart (default: 300)") do |num|
              @options[:expire] = num
            end

            opts.on("-m", "--max-restarts NUM", Integer, "maximum number of restarts before relocation (default: 3)") do |num|
              @options[:max_restarts] = num
            end

            opts.on("--new-cluster", "create a new cluster even it one already exists") do
              @options[:new_cluster] = true
            end
          end

          def get_services
            # Get current services from ccs
            s = Hash.new {|h,k| h[k] = Hash.new {|h1,k1| h1[k1] = {}}}
            cservice = Struct.new(:name, :type)
            cl = []
            service, domain, spool, name = nil, nil, nil, nil
            run = exec_ccs("--lsservices")
            exit!(1, "Unable to determine services configured in the cluster") if (not run.success) && (not @options.has_key?(:wallaby_only))
            run.output.each do |line|
              if (not line.scan(/^service:.*/).empty?) || (not line.scan(/^resources:.*/).empty?)
                if domain && spool && name && (not cl.empty?) && service
                  s[name][:service] = service
                  s[name][:domain] = domain
                  s[name][:spool] = spool
                  s[name][:condor] = cl
                end
                service, domain, spool, name = nil, nil, nil, nil
                cl = []
              end
              service = $1.chomp if line =~ /^service:\s*name=([^,]+),/
              domain = $1.chomp if line =~ /domain=([^,]+),/
              spool = $1.chomp if line =~ /mountpoint=([^,]+),/
              cl << cservice.new($1.chomp, $2.chomp) if line =~ /condor:\s*name=([^,]+).*type=([^,]+)/
              name = $1.chomp if line =~ /condor:\s*name=([^,]+).*type=schedd/
            end

            # Add the final service if it exists
            if domain && spool && name && (not cl.empty?) && service
              s[name][:service] = service
              s[name][:domain] = domain
              s[name][:spool] = spool
              s[name][:condor] = cl
            end
            domain, spool, name = nil, nil, nil
            cl = []

            s
          end

          def get_domains
            # Get current failover domains from ccs
            name = ""
            d = Hash.new {|h,k| h[k] = []}
            run = exec_ccs("--lsfailoverdomain")
            exit!(1, "Unable to determine failover domains configured in the cluster") if (not run.success) && (not @options.has_key?(:wallaby_only))
            run.output.each do |line|
              line.scan(/^(\S[^:]+)/) {|l| name = l[0]}
              line.scan(/^\s+([^:]+)/) {|l| d[name] << l[0]}
            end
            d
          end

          def services
            @services ||= get_services
          end

          def domains
            @domains ||= get_domains
          end
        
          def action
            self.class.opname.split("-", 2).last.gsub(/-/, '_').to_sym
          end

          def subsys
            self.class.opname.split("-").last.gsub(/(server)/, '_\1')
          end

          def parse_args(*args)
            @action = action
            @subsys = subsys
            valid_args = cmd_args[@action]
            exit!(1, "Too few arguments") if args.length < valid_args.length

            args.each do |arg|
              pair = arg.split('=', 2)
              if pair.length > 1
                arg_name = pair[0].downcase.to_sym
                exit!(1, "Argument #{pair[0]} is not valid") if not valid_args.include?(arg_name)
                @options[arg_name] = pair[1]
              else
                @options.has_key?(:nodes) ? @options[:nodes].push(pair[0]) : @options[:nodes] = [pair[0]]
              end
            end

            valid_args.each do |arg|
              exit!(1, "No #{arg} provided") if not @options.has_key?(arg)
            end

            @name = @options[:name]
            def_sname = "HA Schedd #{@name}"
            def_dname = "Schedd #{@name} Failover Domain"

            if not @options.has_key?(:wallaby_only)
              @daemon_name = (services.has_key?(@name) ? services[@name][:condor].collect {|d| d.name if d.type == @subsys}.compact.to_s : nil)
              @service = (services.has_key?(@name) ? services[@name][:service] : def_sname)
              @domain = (services.has_key?(@name) ? services[@name][:domain] : def_dname)
            else
              @daemon_name = nil
              @service = def_sname
              @domain = def_dname
            end

            @tag = "CLUSTER_NAMES"
          end

          def group_name
            "Automatically generated High-Availability configuration for Schedd #{@name}"
          end

          def self.included(receiver)
            if receiver.respond_to?(:register_callback)
              receiver.register_callback :after_option_parsing, :parse_args
            end
          end

          def ignore_arg
            # Determine if -i is needed
            `ccs -h localhost -i`
            $?.exitstatus == 0
          end

          def password
            "--password #{@options[:password]}" if @options.has_key?(:password)
          end

          def exec_ccs(args)
            @timeout ||= @options.has_key?(:timeout) ? @options[:timeout] : 60
            @ignore ||= "-i" if ignore_arg

            result = Struct.new(:success, :output).new
            begin
              Timeout.timeout(@timeout) do
                result.output = `ccs #{@ignore} -h localhost #{password} #{args}`
                result.success = $?.exitstatus == 0
              end
            rescue Timeout::Error
              result.success = false
            end
            result
          end

          def modify_ccs_nodes
            cmd = (@action.to_s.include?("remove") ? "rm" : "add")
            @options[:nodes].each do |node|
              # Failure to add means the node is already part of the
              # cluster, so we don't care about a failure.
              exec_ccs("--#{cmd}node #{node}") if cmd == "add"
              exit!(1, "Unable to #{cmd} failover domain node #{node} #{preposition} the HA schedd #{@name} failover domain") if not exec_ccs("--#{cmd}failoverdomainnode '#{@domain}' #{node}").success
            end
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
            Mrg::Grid::Config::Shell.constants.grep(/^Cluster#{@subsys.split('_').collect{|n| n.capitalize}}Params/)[0].to_sym
          end

          def get_params
            Mrg::Grid::Config::Shell.const_get(get_const_name).gen_params(@name, @options[:spool], @daemon_name)
          end

          def get_store_feature
            Mrg::Grid::Config::Shell.const_get(get_const_name).store_feature
          end

          def add_params_to_store
            # Add Parameters if they don't exist
            store.checkParameterValidity(get_params.keys).each do |pname|

            # Add missing params
              pargs = ["--needs-restart", get_restart(pname), "--kind", get_kind(pname), "--description", "Created for HA Schedd #{@name}", pname]
              Mrg::Grid::Config::Shell::AddParam.new(store, "").main(pargs)
            end
          end

          def get_kind(n)
            n.upcase.include?("PROCD") ? "Boolean" : "String"
          end

          def get_restart(n)
            n.upcase.include?("RESTART_PROCD") ? "no" : "yes"
          end

          def modify_params_on_group
            param_list = []
            cmd = (@action.to_s.include?("remove") ? @action.to_s.split('_', 2).first.capitalize : "Add")
            get_params.each_pair do |n, v|
              cmd.include?("Add") ? param_list << "#{n}=#{v}" : param_list << "#{n}"
            end
            Mrg::Grid::Config::Shell.const_get("#{cmd}GroupParam").new(store, "").main([group_name] + param_list)
            Mrg::Grid::Config::Shell::const_get("#{cmd}GroupFeature").new(store, "").main([group_name] + [get_store_feature])
            mark_group
          end

          def update_store
            # Add groups to the store
            add_group_to_store

            # Add params to the store
            add_params_to_store

            # Add params to the group
            modify_params_on_group

            # Add nodes to the group
            modify_group_membership
          end
        end

        # Schedd related commands
        class ClusterCreate < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps

          def self.opname
            "cluster-create"
          end
        
          def self.description
            "Create an HA Schedd cluster configuration"
          end
        
          def supports_add
            true
          end

          def subsys
            "schedd"
          end

          def act
            if not @options.has_key?(:wallaby_only)
              restarts = @options.has_key?(:max_restarts) ? @options[:max_restarts] : 3
              expire = @options.has_key?(:expire) ? @options[:expire] : 300

              # Cluster config
              if @options.has_key?(:new_cluster)
                exit!(1, "Failed to stop all cluster services") if not exec_ccs("--stopall").success
                exit!(1, "Failed to create the cluster.  Try supplying a ricci password with --riccipassword") if not exec_ccs("--createcluster 'HACondorCluster'").success
              end

              # Handle nodes
              exit!(1, "Failed to add failover domain for HA schedd #{@name}.  Does it already exist?") if not exec_ccs("--addfailoverdomain '#{@domain}' restricted").success
              modify_ccs_nodes

              # Handle service
              exit!(1, "Failed to add HA schedd #{@name} cluster service.  Does it already exist?") if not exec_ccs("--addservice '#{@service}' domain='#{@domain}' autostart=1 recovery=relocate").success

              # Handle subservices
              exit!(1, "Failed to add shared storage to HA schedd #{@name} service") if not exec_ccs("--addsubservice '#{@service}' netfs name='Job Queue for #{@options[:name]}' mountpoint='#{@options[:spool]}' host='#{@options[:server]}' export='#{@options[:export]}' options='rw,soft' force_unmount='on'").success
              exit!(1, "Failed to add condor instance to HA schedd #{@name} service") if not exec_ccs("--addsubservice '#{@service}' netfs:condor name='#{@options[:name]}' type='#{@subsys}' __independent_subtree='1' __max_restarts=#{restarts} __restart_expire_time=#{expire}").success
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
          include ClusterCommonOps

          def self.opname
            "cluster-delete"
          end
        
          def self.description
            "Delete an HA Schedd cluster configuration"
          end

          def subsys
            "schedd"
          end

          def act
            if @options.has_key?(:nodes)
              puts "Warning: nodes are ignored for delete operations"
            end

            if not @options.has_key?(:wallaby_only)
              # Cluster config
              exit!(1, "Failed to remove HA schedd #{@name} service '#{@service}'") if not exec_ccs("--rmservice '#{@service}'").success
              exit!(1, "Failed to remove HA schedd #{@name} failover domain '#{@domain}'") if not exec_ccs("--rmfailoverdomain '#{@domain}'").success
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
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-add-node"
          end
        
          def self.description
            "Add a node to an existing HA Schedd cluster configuration"
          end
        end

        class ClusterRemoveNode < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterNodeOps

          def self.opname
            "cluster-remove-node"
          end
        
          def self.description
            "Remove a node from an existing HA Schedd cluster configuration"
          end
        end

        class ClusterAddJobServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterServerOps

          def self.opname
            "cluster-add-jobserver"
          end
        
          def self.description
            "Add a JobServer to an existing HA Schedd cluster configuration"
          end
        end

        class ClusterRemoveJobServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterServerOps

          def self.opname
            "cluster-remove-jobserver"
          end
        
          def self.description
            "Remove a JobServer from an existing HA Schedd cluster configuration"
          end
        end

        class ClusterAddQueryServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterServerOps

          def self.opname
            "cluster-add-queryserver"
          end
        
          def self.description
            "Add a QueryServer to an existing HA Schedd cluster configuration"
          end
        end

        class ClusterRemoveQueryServer < ::Mrg::Grid::Config::Shell::Command
          include ClusterCommonOps
          include ClusterServerOps

          def self.opname
            "cluster-remove-queryserver"
          end
        
          def self.description
            "Remove a QueryServer from an existing HA Schedd cluster configuration"
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

          def act
            domain, spool, name, type = nil, nil, nil, nil

            # Remove all groups in the store
            remove_store_groups

            services.keys.each do |n|
              @daemon_name = nil
              @name = n
              @options[:spool] = services[n][:spool]
              @options[:nodes] = domains[services[n][:domain]]
              services[n][:condor].each do |daemon|
                @daemon_name = daemon.name
                @subsys = daemon.type
                update_store
              end
            end

            activate_changes
            return 0
          end
        end
      end
    end
  end
end
