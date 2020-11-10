REGISTRY:=hub.docker.com
DATE:=$(shell date +%Y%m%d)
VERSION:=
SUFFIX:=
NOCACHE:=no
UNVERSIONED:=no
BUILDAH:=no

SHELL:=/bin/bash

basetargets:=build-base-el7  #build-base-el8
imgtargets-el7:=$(shell echo build-{cm,submit,execute,mini}-el7)
#imgtargets-el8:=$(shell echo build-{cm,submit,execute,mini}-el8)
imgtargets:=$(imgtargets-el7)  #$(imgtargets-el8)
pushtargets:=$(shell echo push-{base,cm,submit,execute,mini}-el7)  #$(shell echo push-{base,cm,submit,execute,mini}-el8)
pushalltargets:=$(shell echo push-{base,cm,submit,execute,mini}-el7)  #$(shell echo push-{base,cm,submit,execute,mini}-el8)


echo:=@echo
echotbl:=@printf "%-30s %s\n"


buildcmd:=$(if $(findstring yes,$(BUILDAH)),\
	buildah build-using-dockerfile,\
	docker build)

define docker_build =
$(buildcmd) \
  --build-arg VERSION=$(VERSION) \
  --build-arg EL=$(2) \
  --build-arg BUILDDATE=$(DATE) \
  --build-arg SUFFIX=$(SUFFIX) \
  $(if $(findstring yes,$(NOCACHE)),--no-cache,) \
  -t htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX) \
  $(if $(findstring yes,$(UNVERSIONED)),-t htcondor/$(1):el$(2)$(SUFFIX),) \
  -f $(1)/Dockerfile \
  .
endef


define docker_push_versioned =
$(if $(findstring hub.docker.com,$(REGISTRY)),\
	$(call docker_push_hub_versioned,$(1),$(2)),\
	$(call docker_push_custom_versioned,$(1),$(2)))
endef


define docker_push_unversioned =
$(if $(findstring hub.docker.com,$(REGISTRY)),\
	$(call docker_push_hub_unversioned,$(1),$(2)),\
	$(call docker_push_custom_unversioned,$(1),$(2)))
endef


define docker_push_hub_versioned =
docker push htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX)
endef


define docker_push_custom_versioned =
docker tag              htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX) \
	    $(REGISTRY)/htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX)
docker push $(REGISTRY)/htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX)
-docker rmi $(REGISTRY)/htcondor/$(1):$(VERSION)-el$(2)$(SUFFIX)
endef


define docker_push_hub_unversioned =
docker push htcondor/$(1):el$(2)$(SUFFIX)
endef


define docker_push_custom_unversioned =
docker tag              htcondor/$(1):el$(2)$(SUFFIX) \
	    $(REGISTRY)/htcondor/$(1):el$(2)$(SUFFIX)
docker push $(REGISTRY)/htcondor/$(1):el$(2)$(SUFFIX)
-docker rmi $(REGISTRY)/htcondor/$(1):el$(2)$(SUFFIX)
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
	$(echotbl) "tag-unversioned" "Add unversioned tags of the images for the given VERSION"
	$(echotbl) "" "(e.g. 'tag-unversioned VERSION=8.9.5' makes an"
	$(echotbl) "" "htcondor/mini:el7 tag from htcondor/mini:8.9.5-el7)"
	$(echotbl) "clean" "Delete images from local cache"
	$(echotbl) "clean-stamps" "Delete build stamps only, causing docker builds"
	$(echotbl) "" "to be run next time (but leaving the cache alone)"
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
	$(echotbl) "UNVERSIONED" "Set to 'yes' to make or push unversioned tags of the images"
	$(echotbl) "" "(e.g. htcondor/mini:el7 -> htcondor/mini:VERSION-el7) [$(UNVERSIONED)]"
	$(echotbl) "BUILDAH" "Set to 'yes' to build using buildah instead of docker [$(BUILDAH)]"


.PHONY: build
build: $(imgtargets)


# "build-cm-el7" -> "build" "cm" "el7"
split = $(subst -, ,$(1))
# "cm"
img = $(word 2, $(call split,$(1)))
# "el7" -> "7"
el = $(subst el,,$(word 3,$(call split,$(1))))

# define-build-target(target)
#  Defines build-* target (e.g. build-cm-el7) and target for the associated stamp file (e.g. .build-cm-el7.stamp).
#  Check that the user specified VERSION, docker build the image, and create the stamp file if successful.
define define-build-target

.PHONY: $(1)
# Create a "stamp" file for the build target
$(1): .$(1).stamp

# Stamp file depends on the image directory
.$(1).stamp: $$(call img,$(1))/

# Error if the user didn't specify VERSION
	@: $$(if $$(value VERSION),,$$(error VERSION not set))

# Build the image
	$$(call docker_build,$$(call img,$(1)),$$(call el,$(1)))

# Create the stamp file
	touch $$@

endef


# add-base-dependency(target)
#  Adds to a build-* target a dependency on the appropriate base image (by means of its stamp file).
#  For example, add-base-dependency(build-cm-el7) will add
#    .build-cm-el7.stamp: .build-base-el7.stamp
define add-base-dependency

.$(1).stamp: .build-base-el$$(call el,$(1)).stamp

endef


$(foreach it, $(basetargets) $(imgtargets),\
	$(eval $(call define-build-target,$(it))))

$(foreach it, $(imgtargets),\
	$(eval $(call add-base-dependency,$(it))))


# "build-base-el7" -> "base"
getimg=$(word 2,$(subst -, ,$@))
# "build-base-el7" -> "7"
getel=$(subst .stamp,,$(subst el,,$(word 3,$(subst -, ,$@))))


.PHONY: push push-all
push: $(pushtargets)
push-all: $(pushalltargets)

.PHONY: $(pushalltargets)
$(pushalltargets):
	$(if $(value VERSION),\
		$(call docker_push_versioned,$(getimg),$(getel)),)
	$(if $(findstring yes,$(UNVERSIONED)),\
		$(call docker_push_unversioned,$(getimg),$(getel)),)


define clean_versioned =
-docker rmi htcondor/{base,cm,execute,mini,submit}:$(VERSION)-el$(1)$(SUFFIX)
endef


define clean_unversioned =
-docker rmi htcondor/{base,cm,execute,mini,submit}:el$(1)$(SUFFIX)
endef


fulltag_from_target = htcondor/$(call img,$(1)):$(VERSION)-el$(call el,$(1))$(SUFFIX)
fulltag_uv_from_target = htcondor/$(call img,$(1)):el$(call el,$(1))$(SUFFIX)


.PHONY: tag-unversioned
tag-unversioned:
	@: $(if $(value VERSION),,$(error VERSION not set))

	$(foreach it,$(basetargets) $(imgtargets),\
		docker tag $(call fulltag_from_target,$(it)) $(call fulltag_uv_from_target,$(it));)


.PHONY: clean-stamps
clean-stamps:
	$(foreach it,$(basetargets) $(imgtargets),\
		rm -f .$(it).stamp;)

.PHONY: clean
clean: clean-stamps
	$(if $(value VERSION),\
		$(call clean_versioned,7),)
	$(if $(findstring yes,$(UNVERSIONED)),\
		$(call clean_unversioned,7),)
#	$(if $(value VERSION),\
#		$(call clean_versioned,8),)
#	$(if $(findstring yes,$(UNVERSIONED)),\
#		$(call clean_unversioned,8),)

