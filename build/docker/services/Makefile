REGISTRY:=hub.docker.com
DATE:=$(shell date +%Y%m%d)
VERSION:=8.9.5
SUFFIX:=
NOCACHE:=no
UNVERSIONED:=no

basetargets:=build-base-el7  #build-base-el8
imgtargets-el7:=$(shell echo build-{cm,submit,execute,mini}-el7)
#imgtargets-el8:=$(shell echo build-{cm,submit,execute,mini}-el8)
imgtargets:=$(imgtargets-el7)  #$(imgtargets-el8)
pushtargets:=$(shell echo push-{execute,mini}-el7)  #$(shell echo push-{execute-mini}-el8)
pushalltargets:=$(shell echo push-{base,cm,submit,execute,mini}-el7)  #$(shell echo push-{base,cm,submit,execute,mini}-el8)


echo:=@echo
echotbl:=@printf "%-30s %s\n"

define dockerbuild =
docker build \
  --build-arg VERSION=$(VERSION) \
  --build-arg EL=$(2) \
  --build-arg BUILDDATE=$(DATE) \
  $(if $(findstring yes,$(NOCACHE)),--no-cache,) \
  -t htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX) \
  . -f $(1)/Dockerfile
$(if $(findstring yes,$(UNVERSIONED)),docker tag htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX) htcondor/$(1):el$(2)$(SUFFIX),)
endef


define dockerpush =
docker tag htcondor/$(1):$(2)$(SUFFIX) $(REGISTRY)/htcondor/$(1):$(2)$(SUFFIX)
docker push $(REGISTRY)/htcondor/$(1):$(2)$(SUFFIX)
-docker rmi $(REGISTRY)/htcondor/$(1):$(2)$(SUFFIX)
endef


define dockerpushhub =
docker push htcondor/$(1):$(2)$(SUFFIX)
endef


.PHONY: help
help:
	$(echo) "Build and push Docker images for HTCondor"
	$(echo)
	$(echo) "Targets:"
	$(echo)
	$(echotbl) "build" "Build all distro/image combinations"
	$(echotbl) "build-IMAGE-DISTRO" "Build one distro/image combination"
	$(echotbl) "push" "Push default distro/image combinations to a registry"
	$(echotbl) "push-all" "Push all distro/image combinations to a registry"
	$(echotbl) "push-IMAGE-DISTRO" "Push one distro/image combination to a registry"
	$(echotbl) "clean" "Delete images from local cache"
	$(echotbl) "help" "This text"
	$(echo)
	$(echo) "Available build targets:"
	$(echo) "  $(basetargets)"
	$(echo) "  $(imgtargets-el7)"
#	$(echo) "  $(imgtargets-el8)"
	$(echo)
	$(echo) "Default push targets:"
	$(echo) "  $(pushtargets)"
	$(echo)
	$(echo) "All push targets:"
	$(echo) "  $(pushalltargets)"
	$(echo)
	$(echo)
	$(echo) "Variables:"
	$(echo)
	$(echotbl) "REGISTRY" "Docker registry to push to [$(REGISTRY)]"
	$(echotbl) "NOCACHE" "Set to 'yes' to disable use of Docker cache for building [$(NOCACHE)]"
	$(echotbl) "SUFFIX" "Text to append to tag"
	$(echotbl) "VERSION" "HTCondor version [$(VERSION)]"
	$(echotbl) "UNVERSIONED" "Set to 'yes' to make unversioned tags of the images"
	$(echotbl) "" "(e.g. htcondor/mini:el7 -> htcondor/mini:$(VERSION)-el7) [$(UNVERSIONED)]"


.PHONY: build
build: $(imgtargets)

# "build-base-el7" -> "base"
getimg=$(word 2,$(subst -, ,$@))
# "build-base-el7" -> "7"
getel=$(subst el,,$(word 3,$(subst -, ,$@)))

.PHONY: $(basetargets)
$(basetargets):
	$(call dockerbuild,$(getimg),$(getel))

.PHONY: $(imgtargets)
$(imgtargets):
	$(call dockerbuild,$(getimg),$(getel))

$(imgtargets-el7): build-base-el7
#$(imgtargets-el8): build-base-el8

.PHONY: push push-all
push: $(pushtargets)
push-all: $(pushalltargets)

.PHONY: $(pushalltargets)
$(pushalltargets):
	@: $(if $(value REGISTRY),,$(error REGISTRY not set))
	$(if $(findstring hub.docker.com,$(REGISTRY)),$(call dockerpushhub,$(getimg),$(VERSION)-el$(getel)),$(call dockerpush,$(getimg),$(VERSION)-el$(getel)))
	$(if $(findstring yes,$(UNVERSIONED)),$(if $(findstring hub.docker.com,$(REGISTRY)),$(call dockerpushhub,$(getimg),el$(getel)),$(call dockerpush,$(getimg),el$(getel))),)

.PHONY: clean
clean:
	-docker rmi htcondor/{base,cm,execute,mini,submit}:$(VERSION)-el7$(SUFFIX)
	-docker rmi htcondor/{base,cm,execute,mini,submit}:el7$(SUFFIX)
#	-docker rmi htcondor/{base,cm,execute,mini,submit}:$(VERSION)-el8$(SUFFIX)
#	-docker rmi htcondor/{base,cm,execute,mini,submit}:el8$(SUFFIX)

